using Microsoft.ML.OnnxRuntime;
using Microsoft.ML.OnnxRuntime.Tensors;
using Newtonsoft.Json;
using OpenCvSharp;
using System.Text;

namespace Work1
{
    /// <summary>
    /// 검출 결과
    /// </summary>
    public class DetectBox
    {
        public string ClassName;
        public float Confidence;
        public int X, Y, W, H;
    }

    /// <summary>
    /// ★ Python 검출 로직을 전부 C#으로 포팅한 클래스
    /// 
    /// 필요한 NuGet 패키지:
    ///   - OpenCvSharp4
    ///   - OpenCvSharp4.Extensions
    ///   - OpenCvSharp4.runtime.win
    ///   - Microsoft.ML.OnnxRuntime  (또는 .Gpu)
    /// </summary>
    public class DefectDetector : IDisposable
    {
        // ONNX 세션
        private InferenceSession session;
        private string inputName;

        // ──── 설정값 ────
        public float ConfThreshold = 0.35f;
        public int ImgSize = 640;
        public float NmsThreshold = 0.45f;
        public string[] ClassNames = { "hole", "thread", "stain" };

        public float RoiWRatio = 0.65f;
        public float RoiHRatio = 0.60f;

        public class FabricInfo
        {
            public bool IsPresent;
            public OpenCvSharp.Point2f Center;
            public double Area;
            public double AreaRatio;
        }

        public void ResetState()
        {
            _consecutiveDefects = 0;
            _cachedBoxes.Clear();
            _cachedDefectList.Clear();
            _cachedMode = "";
            _skipCounter = 0;
            _isConfirmed = false;
        }

        // 색상 (BGR)
        public static readonly Dictionary<string, Scalar> Colors = new()
        {
            ["hole"] = new Scalar(0, 0, 255),
            ["thread"] = new Scalar(0, 165, 255),
            ["stain"] = new Scalar(255, 0, 0),
            ["fabric"] = new Scalar(0, 255, 0),
        };

        // Fabric
        public int FabricBinThresh = 80;
        public int FabricMinArea = 5000;   // ★ 1500 → 5000 (작은 노이즈 영역 무시)

        // Hole
        public int HoleDarkDiff = 20;
        public int HoleMaxV = 110;
        public int HoleMaxS = 120;
        public int HoleMinArea = 35;
        public float HoleMaxAspect = 1.8f;
        public float HoleMinFillRatio = 0.45f;
        public float HoleMaxAreaRatio = 0.18f;
        public float HoleMinCircularity = 0.30f;

        // Thread
        public int ThreadBrightMin = 155;
        public int ThreadHoughThreshold = 18;
        public int ThreadMinLineLength = 16;
        public int ThreadMaxLineGap = 4;
        public int ThreadLineThickness = 2;
        public int ThreadMinCompArea = 20;
        public float ThreadMinCompAspect = 3.2f;
        public int ThreadMaxCompThickness = 10;
        public float ThreadMinFillRatio = 0.22f;
        public int ThreadOuterErodeKernel = 5;

        // Stain
        public int StainHDiff = 8;        // ★ 재검증 결과: 8
        public int StainSDiff = 78;       // ★ 재검증 결과: 78 (100→78)
        public int StainVDiff = 30;       // ★ 재검증 결과: 30 (46→30)
        public int StainMinArea = 800;    // ★ 재검증 결과: 800 (200→800, false positive 차단의 핵심)
        public float StainMaxAreaRatio = 0.20f;
        public int StainBrightExcludeV = 255;  // ★ 재검증 결과: 255 (242→255, 밝은 옷감도 stain 검출 가능)

        public bool ShowDebugMask = true;
        public bool UseYoloStain = true;

        public int FabricMinV = 100;   // 컨베이어 차단용 — S 임계가 메인 게이트라 V는 좀 풀어줌
        public int FabricMinS = 60;    // ★ 12 → 60 (채도 있는 옷감만, 그림자/회색 하이라이트 차단)
        public float FabricMinAreaRatio = 0.25f;  // ★ 0.10 → 0.25 (작은 노이즈 영역 차단, ROI의 25% 이상이어야 진짜 옷감)

        // ──── 생성자 ────
        public DefectDetector(string onnxPath)
        {
            if (!System.IO.File.Exists(onnxPath))
                throw new System.IO.FileNotFoundException($"ONNX 모델 없음: {onnxPath}");

            var opts = new SessionOptions();
            opts.GraphOptimizationLevel = GraphOptimizationLevel.ORT_ENABLE_ALL;
            session = new InferenceSession(onnxPath, opts);
            inputName = session.InputMetadata.Keys.First();
        }

        // ══════════════════════════════════════
        // ★ 검출용 최대 크기 (이보다 크면 축소 후 검출 → 좌표 확대)
        // 작을수록 빠름. 0이면 축소 안 함.
        // ══════════════════════════════════════
        public int ProcessMaxWidth = 0;    // 0이면 축소 안 함 (정확도 유지)
        public int ProcessMaxHeight = 0;

        // ══════════════════════════════════════
        // ★ 불량 확정 캐시: N프레임 연속 검출 → 확정 → 스킵
        // ══════════════════════════════════════
        public int ConfirmAfterN = 15;         // 연속 몇 프레임 검출되면 확정
        public int SkipAfterConfirm = 30;      // 확정 후 몇 프레임 검출 안 하고 캐시 사용

        private int _consecutiveDefects = 0;
        private List<DetectBox> _cachedBoxes = new();
        private List<(string name, float conf)> _cachedDefectList = new();
        private string _cachedMode = "";
        private int _skipCounter = 0;
        private bool _isConfirmed = false;

        public FabricInfo DetectFabricInRoi(Mat frame)
        {
            var info = new FabricInfo
            {
                IsPresent = false,
                Center = new OpenCvSharp.Point2f(0, 0),
                Area = 0,
                AreaRatio = 0
            };

            if (frame == null || frame.Empty())
                return info;

            var (rx1, ry1, rx2, ry2) = GetCenterRoi(frame);

            using var roi = new Mat(frame, new Rect(rx1, ry1, rx2 - rx1, ry2 - ry1));
            using var hsv = new Mat();

            Cv2.CvtColor(roi, hsv, ColorConversionCodes.BGR2HSV);

            // ★ 채도 있는 밝은 옷감만 통과 (초록/분홍 옷감 OK, 그림자/회색 하이라이트 차단)
            //   lightGrayMask(무채색 통과) 제거 — 그림자가 무채색이라 통과되던 문제 해결
            using var fabricMask = new Mat();
            Cv2.InRange(
                hsv,
                new Scalar(0, FabricMinS, FabricMinV),
                new Scalar(179, 255, 255),
                fabricMask
            );

            using var kernel = Cv2.GetStructuringElement(
                MorphShapes.Rect,
                new OpenCvSharp.Size(5, 5)
            );

            Cv2.MorphologyEx(fabricMask, fabricMask, MorphTypes.Open, kernel);
            Cv2.MorphologyEx(fabricMask, fabricMask, MorphTypes.Close, kernel);

            Cv2.FindContours(
                fabricMask,
                out OpenCvSharp.Point[][] contours,
                out _,
                RetrievalModes.External,
                ContourApproximationModes.ApproxSimple
            );

            if (contours.Length == 0)
                return info;

            double maxArea = 0;
            OpenCvSharp.Point[]? best = null;

            foreach (var c in contours)
            {
                double area = Cv2.ContourArea(c);
                if (area > maxArea)
                {
                    maxArea = area;
                    best = c;
                }
            }

            double roiArea = Math.Max(1, roi.Width * roi.Height);
            double areaRatio = maxArea / roiArea;

            if (best == null || maxArea < FabricMinArea || areaRatio < FabricMinAreaRatio)
                return info;

            var m = Cv2.Moments(best);

            if (Math.Abs(m.M00) < 1e-5)
                return info;

            info.IsPresent = true;
            info.Center = new OpenCvSharp.Point2f(
                (float)(rx1 + m.M10 / m.M00),
                (float)(ry1 + m.M01 / m.M00)
            );
            info.Area = maxArea;
            info.AreaRatio = areaRatio;

            return info;
        }

        // ══════════════════════════════════════
        // ★ 메인 처리 함수
        // ══════════════════════════════════════
        private static readonly HttpClient _httpClient = new HttpClient
        {
            Timeout = TimeSpan.FromMilliseconds(500) // 0.5초 안에 응답 없으면 포기 (분석 루프 보호)
        };

        private bool _isNormalSent = false;
        private bool _isDefectSent = false; // 결함 전송 여부도 관리
        public async Task SendDefectData(string defectType, List<float> confidences, Dictionary<string, int> counts)
        {
            // 이미 이 결함 상태를 전송 중이거나 성공했다면 중복 호출 방지
            if (_isDefectSent) return;

            try
            {
                string url = "http://192.168.0.30:5000/predict";

                var data = new
                {
                    type = defectType,
                    confidence = string.Join(", ", confidences.Select(c => c.ToString("F2"))),
                    counts = counts,
                    total_count = counts.Values.Sum(),
                    time = DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss")
                };

                var json = JsonConvert.SerializeObject(data);
                var content = new StringContent(json, Encoding.UTF8, "application/json");

                // [핵심] Task.Run을 사용하여 분석 루프와 전송 루프를 완전히 분리합니다.
                // 이렇게 하면 네트워크가 느려져도 C# 분석 속도(FPS)는 떨어지지 않습니다.
                _ = Task.Run(async () =>
                {
                    try
                    {
                        var response = await _httpClient.PostAsync(url, content);
                        if (response.IsSuccessStatusCode)
                        {
                            _isDefectSent = true;   // 전송 성공 시 플래그 설정
                            _isNormalSent = false;  // 정상 신호 플래그는 리셋
                            Console.WriteLine($"[{DateTime.Now:HH:mm:ss}] 결함 데이터 전송 성공");
                        }
                    }
                    catch (Exception ex)
                    {
                        // 전송 실패해도 분석은 계속되어야 하므로 로그만 남김
                        Console.WriteLine($"네트워크 지연 또는 에러: {ex.Message}");
                    }
                });
            }
            catch (Exception ex)
            {
                Console.WriteLine($"데이터 준비 오류: {ex.Message}");
            }
        }
        public (List<DetectBox> boxes, bool isDefect, bool isNormal, List<(string name, float conf)> defectList, string mode) ProcessFrame(Mat frame)
        {
            // 1. 불량 확정 상태면 캐시 반환 (성능 최적화)
            if (_isConfirmed)
            {
                _skipCounter++;
                if (_skipCounter < SkipAfterConfirm)
                {
                    return (_cachedBoxes, true, false, _cachedDefectList, _cachedMode + "*");
                }
                _isConfirmed = false;
                _skipCounter = 0;
            }
            var fabricInfo = DetectFabricInRoi(frame);
            bool isPresent = fabricInfo.IsPresent;


            // 2. ROI 및 리사이즈 처리
            var (rx1, ry1, rx2, ry2) = GetCenterRoi(frame);
            using var roi = new Mat(frame, new Rect(rx1, ry1, rx2 - rx1, ry2 - ry1));

            int origW = roi.Width, origH = roi.Height;
            bool needResize = ProcessMaxWidth > 0 && ProcessMaxHeight > 0
                              && (origW > ProcessMaxWidth || origH > ProcessMaxHeight);

            float scaleX = 1f, scaleY = 1f;
            Mat processRoi;
            if (needResize)
            {
                float ratio = Math.Min((float)ProcessMaxWidth / origW, (float)ProcessMaxHeight / origH);
                int newW = (int)(origW * ratio), newH = (int)(origH * ratio);
                processRoi = new Mat();
                Cv2.Resize(roi, processRoi, new OpenCvSharp.Size(newW, newH));
                scaleX = (float)origW / newW;
                scaleY = (float)origH / newH;
            }
            else { processRoi = roi; }

            // 3. [검출 및 필터링 로직]
            // 파일 내의 DetectPriority를 호출하여 모든 후보를 가져옵니다.
            var (allResults, _) = DetectPriority(processRoi, 0, 0);

            List<DetectBox> finalBoxes = new List<DetectBox>();
            string finalMode = "NONE";

            // --- 핵심: Thread와 Hole은 같이 취급 (동시 인식) ---
            var priorityItems = allResults.Where(b => b.ClassName == "thread" || b.ClassName == "hole").ToList();

            if (priorityItems.Count > 0)
            {
                // Thread나 Hole이 하나라도 있으면 이들만 결과에 포함 (둘 다 있으면 둘 다 포함됨)
                finalBoxes = priorityItems;

                // 텍스트박스에 "thread, hole" 처럼 표시하기 위한 모드 설정
                var uniqueNames = finalBoxes.Select(b => b.ClassName).Distinct();
                finalMode = string.Join(", ", uniqueNames).ToUpper();
            }
            else
            {
                // Thread와 Hole이 아예 없을 때만 Stain(얼룩) 확인
                var stains = allResults.Where(b => b.ClassName == "stain").ToList();
                if (stains.Count > 0)
                {
                    finalBoxes = stains;
                    finalMode = "STAIN";
                }
            }

            // 4. 좌표 변환
            foreach (var box in finalBoxes)
            {
                box.X = (int)(box.X * scaleX) + rx1;
                box.Y = (int)(box.Y * scaleY) + ry1;
                box.W = (int)(box.W * scaleX);
                box.H = (int)(box.H * scaleY);
            }

            var defectList = finalBoxes.Select(b => (b.ClassName, b.Confidence)).ToList();
            bool isDefect = finalBoxes.Count > 0;
            bool isNormal = isPresent && !isDefect;

            // 5. 연속 검출 및 캐싱

            if (isDefect)
            {
                _consecutiveDefects++;
                if (_consecutiveDefects >= ConfirmAfterN)
                {
                    // 확정되는 첫 프레임에만 전송
                    if (!_isConfirmed)
                    {
                        // --- 갯수 카운팅 로직 시작 ---
                        var defectCounts = finalBoxes
                            .GroupBy(b => b.ClassName) // 클래스 이름별로 그룹화
                            .ToDictionary(g => g.Key, g => g.Count()); // { "hole": 2, "thread": 1 } 형태의 딕셔너리 생성

                        // 전송용 문자열 및 리스트 준비
                        string typeString = string.Join(", ", defectCounts.Keys); // "hole, thread"
                        var confList = finalBoxes.Select(b => b.Confidence).ToList();
                        // --- 갯수 카운팅 로직 끝 ---

                        // 비동기로 전송 (counts 파라미터 추가)
                        _ = SendDefectData(typeString, confList, defectCounts);
                    }

                    _isConfirmed = true;
                    _skipCounter = 0;
                    _cachedBoxes = finalBoxes;
                    _cachedDefectList = defectList;
                    _cachedMode = finalMode;
                }
            }
            else
            {
                _consecutiveDefects = 0;
            }
            if (needResize) processRoi.Dispose();
            return (finalBoxes, isDefect, isNormal, defectList, finalMode);
        }

        /// <summary>
        /// 프레임에 결과 그리기 (ROI + 바운딩박스 + 상태패널 + 불량률)
        /// ★ defectRate 파라미터 추가 — 비어있으면 안 그림
        /// </summary>
        public Mat DrawAnnotated(Mat frame, List<DetectBox> boxes, bool isDefect, bool isNormal, List<(string name, float conf)> defectList, string mode, string defectRate = "")
        {
            Mat res = new Mat();
            var result = frame.Clone();

            // --- 선명화 로직 시작 ---
            using (Mat blurred = new Mat())
            {
                // 가우시안 블러로 베이스 생성
                Cv2.GaussianBlur(frame, blurred, new OpenCvSharp.Size(0, 0), 3);
                // 원본(1.5배) - 블러(0.5배) = 경계선이 날카로워진 이미지 생성
                Cv2.AddWeighted(frame, 1.5, blurred, -0.5, 0, res);
            }
            // --- 선명화 로직 끝 ---

            // ROI 사각형 — 불량이면 빨강, 아니면 초록
            var (rx1, ry1, rx2, ry2) = GetCenterRoi(result);
            var roiColor = isDefect ? new Scalar(0, 0, 255) : new Scalar(0, 255, 0);
            Cv2.Rectangle(result, new OpenCvSharp.Point(rx1, ry1), new OpenCvSharp.Point(rx2, ry2), roiColor, 2);

            // 바운딩박스
            foreach (var box in boxes)
            {
                var color = Colors.GetValueOrDefault(box.ClassName, new Scalar(255, 255, 255));
                Cv2.Rectangle(result, new OpenCvSharp.Point(box.X, box.Y),
                              new OpenCvSharp.Point(box.X + box.W, box.Y + box.H), color, 2);

                string label = $"{box.ClassName} {box.Confidence:F2}";
                var textSize = Cv2.GetTextSize(label, HersheyFonts.HersheySimplex, 0.6, 1, out _);
                int ytop = Math.Max(0, box.Y - textSize.Height - 10);
                Cv2.Rectangle(result, new OpenCvSharp.Point(box.X, ytop),
                            new OpenCvSharp.Point(box.X + textSize.Width + 4, box.Y), color, -1);
                Cv2.PutText(result, label, new OpenCvSharp.Point(box.X + 2, Math.Max(12, box.Y - 5)),
                            HersheyFonts.HersheySimplex, 0.6, new Scalar(255, 255, 255), 1);
            }

            // 상태 패널
            int h = result.Height, w = result.Width;
            var overlay = result.Clone();
            var panelColor = isDefect ? new Scalar(0, 0, 180) : new Scalar(0, 130, 0);
            Cv2.AddWeighted(overlay, 0.7, result, 0.3, 0, result);
            overlay.Dispose();

            if (isDefect || isNormal)
            {
                string status = isDefect ? "DEFECT" : "OK";
                Scalar textColor = isDefect ? new Scalar(0, 0, 255) : new Scalar(0, 255, 0); // 결함 빨강, 정상 초록

                Cv2.PutText(result, status, new OpenCvSharp.Point(15, 45),
                            HersheyFonts.HersheySimplex, 1.5, textColor, 3);
            }

            // 1. 표시할 텍스트 내용 정의
            string modeText = $"MODE: {mode}";
            // 2. 텍스트의 실제 크기(가로, 세로) 계산
            var modeSize = Cv2.GetTextSize(modeText, HersheyFonts.HersheySimplex, 0.45, 1, out _);
            // 3. 좌표 설정: 
            // X = 화면 너비(w) - 텍스트 가로 길이 - 여백(10)
            // Y = 텍스트 높이 + 여백(20)
            int modeX = w - modeSize.Width - 10;
            int modeY = modeSize.Height + 20;
            // 4. 텍스트 그리기
            Cv2.PutText(result, modeText, new OpenCvSharp.Point(modeX, modeY),
                        HersheyFonts.HersheySimplex, 0.45, new Scalar(255, 255, 0), 1);

            // ★ 우하단에 불량률 표시 (비어있으면 안 그림)
            if (!string.IsNullOrEmpty(defectRate))
            {
                var rateSize = Cv2.GetTextSize(defectRate, HersheyFonts.HersheySimplex, 0.5, 1, out _);
                int rateX = w - rateSize.Width - 12;
                int rateY = h - 12;

                // 가독성을 위한 검정 외곽선(두꺼운 검정 텍스트 → 그 위에 흰 텍스트)
                Cv2.PutText(result, defectRate, new OpenCvSharp.Point(rateX, rateY),
                            HersheyFonts.HersheySimplex, 0.5, new Scalar(0, 0, 0), 3);
                Cv2.PutText(result, defectRate, new OpenCvSharp.Point(rateX, rateY),
                            HersheyFonts.HersheySimplex, 0.5, new Scalar(255, 255, 255), 1);
            }

            return result;
        }


        // ══════════════════════════════════════
        // ROI
        // ══════════════════════════════════════
        public (int x1, int y1, int x2, int y2) GetCenterRoi(Mat frame)
        {
            int h = frame.Height, w = frame.Width;
            int rw = (int)(w * RoiWRatio), rh = (int)(h * RoiHRatio);
            int x1 = (w - rw) / 2, y1 = (h - rh) / 2;
            return (x1, y1, x1 + rw, y1 + rh);
        }

        // ══════════════════════════════════════
        // 검출 우선순위
        // ══════════════════════════════════════
        // ★ YOLO가 못 찾았을 때 Rule/HSV를 몇 프레임마다 실행할지
        public int FallbackEveryN = 5;
        private int _fallbackCounter = 0;

        private (List<DetectBox> boxes, string mode) DetectPriority(Mat roi, int ox, int oy)
        {
            // ★ YOLO와 HSV stain을 병렬로 실행 → 둘 중 더 오래 걸리는 시간만큼만 소요
            //   YOLO는 GPU/CPU 추론 30~50ms, HSV는 CPU 처리 50~100ms
            //   두 작업이 서로 독립적이라 병렬화 안전함

            List<DetectBox>? yoloBoxes = null;
            List<DetectBox>? hsvStains = null;

            // ROI를 두 작업이 공유 — Mat은 thread-safe하지 않으므로 각자 Clone해서 사용
            var roiForYolo = roi.Clone();
            var roiForHsv = roi.Clone();

            try
            {
                Parallel.Invoke(
                    () => { yoloBoxes = RunYolo(roiForYolo, ox, oy); },
                    () => { hsvStains = DetectStainHsv(roiForHsv, ox, oy); }
                );
            }
            finally
            {
                roiForYolo.Dispose();
                roiForHsv.Dispose();
            }

            yoloBoxes ??= new List<DetectBox>();
            hsvStains ??= new List<DetectBox>();

            // 결과 합치기 — YOLO의 stain은 빼고 (HSV가 더 신뢰성 있음), HSV의 stain만 추가
            var combined = yoloBoxes.Where(b => b.ClassName != "stain").ToList();
            combined.AddRange(hsvStains);

            // ★ 클래스 무관 NMS — 같은 위치에 박스가 여러 개면 가장 confidence 높은 1개만
            //   (YOLO가 같은 구멍을 hole 2개로 잡거나, YOLO hole + HSV stain 중복 등을 차단)
            combined = NmsAcrossClasses(combined, 0.3f);

            string mode = combined.Count > 0 ? "MIX" : "NONE";
            return (combined, mode);
        }

        // ══════════════════════════════════════
        // 클래스 무관 NMS — 같은 위치 중복 박스 제거
        // ══════════════════════════════════════
        private List<DetectBox> NmsAcrossClasses(List<DetectBox> boxes, float threshold)
        {
            if (boxes.Count <= 1) return boxes;

            var sorted = boxes.OrderByDescending(b => b.Confidence).ToList();
            var keep = new bool[sorted.Count];
            Array.Fill(keep, true);

            for (int i = 0; i < sorted.Count; i++)
            {
                if (!keep[i]) continue;
                for (int j = i + 1; j < sorted.Count; j++)
                {
                    if (!keep[j]) continue;
                    if (IoU(sorted[i], sorted[j]) > threshold)
                        keep[j] = false;  // 같은 위치, 낮은 confidence 박스 제거
                }
            }

            var result = new List<DetectBox>();
            for (int i = 0; i < sorted.Count; i++)
                if (keep[i]) result.Add(sorted[i]);
            return result;
        }

        // ══════════════════════════════════════
        // YOLO ONNX 추론
        // ══════════════════════════════════════
        private List<DetectBox> RunYolo(Mat roi, int ox, int oy)
        {
            if (session == null) return new();

            // Letterbox
            int origH = roi.Height, origW = roi.Width;
            float ratio = Math.Min((float)ImgSize / origH, (float)ImgSize / origW);
            int newW = (int)(origW * ratio), newH = (int)(origH * ratio);
            int padX = (ImgSize - newW) / 2, padY = (ImgSize - newH) / 2;

            var resized = new Mat();
            Cv2.Resize(roi, resized, new OpenCvSharp.Size(newW, newH));

            var padded = new Mat(ImgSize, ImgSize, MatType.CV_8UC3, new Scalar(114, 114, 114));
            resized.CopyTo(padded[new Rect(padX, padY, newW, newH)]);

            // Mat → Tensor [1, 3, 640, 640]
            var tensor = new DenseTensor<float>(new[] { 1, 3, ImgSize, ImgSize });
            unsafe
            {
                byte* ptr = (byte*)padded.Data;
                for (int y = 0; y < ImgSize; y++)
                    for (int x = 0; x < ImgSize; x++)
                    {
                        int idx = (y * ImgSize + x) * 3;
                        tensor[0, 2, y, x] = ptr[idx + 2] / 255f; // R
                        tensor[0, 1, y, x] = ptr[idx + 1] / 255f; // G
                        tensor[0, 0, y, x] = ptr[idx + 0] / 255f; // B
                    }
            }

            resized.Dispose();
            padded.Dispose();

            // 추론
            var inputs = new List<NamedOnnxValue> { NamedOnnxValue.CreateFromTensor(inputName, tensor) };
            using var results = session.Run(inputs);
            var output = results.First().AsTensor<float>();
            var dims = output.Dimensions.ToArray();

            // [1, 4+nc, N] 파싱
            int numFeats = dims[1];
            int numDets = dims[2];
            int nc = numFeats - 4;

            var rawBoxes = new List<DetectBox>();
            for (int i = 0; i < numDets; i++)
            {
                // 최대 클래스 찾기
                float maxScore = 0; int maxCls = 0;
                for (int c = 0; c < nc; c++)
                {
                    float s = output[0, 4 + c, i];
                    if (s > maxScore) { maxScore = s; maxCls = c; }
                }
                // ★ 클래스별 임계값 분리
                string clsName = maxCls < ClassNames.Length ? ClassNames[maxCls] : maxCls.ToString();

                float effectiveThreshold = ConfThreshold;
                if (clsName == "stain") effectiveThreshold = 0.18f;   // ★ stain은 0.18
                else if (clsName == "hole") effectiveThreshold = 0.20f;  // ★ hole도 0.20 (빛 비친 구멍 잡기)

                if (maxScore < effectiveThreshold) continue;

                if (!UseYoloStain && clsName == "stain") continue;
                if (clsName != "hole" && clsName != "thread" && clsName != "stain") continue;

                // 박스 좌표 (letterbox → 원본)
                float cx = output[0, 0, i], cy = output[0, 1, i];
                float bw = output[0, 2, i], bh = output[0, 3, i];

                float x1f = (cx - bw / 2 - padX) / ratio;
                float y1f = (cy - bh / 2 - padY) / ratio;
                float x2f = (cx + bw / 2 - padX) / ratio;
                float y2f = (cy + bh / 2 - padY) / ratio;

                int bx1 = Math.Max(0, (int)x1f);
                int by1 = Math.Max(0, (int)y1f);
                int bx2 = Math.Min(origW, (int)x2f);
                int by2 = Math.Min(origH, (int)y2f);
                int boxW = bx2 - bx1, boxH = by2 - by1;
                if (boxW <= 0 || boxH <= 0) continue;

                // hole 후처리 (Python과 동일)
                if (clsName == "hole" && by1 < origH && bx1 < origW && boxW > 0 && boxH > 0)
                {
                    var crop = new Mat(roi, new Rect(bx1, by1, boxW, boxH));
                    var hsv = new Mat();
                    Cv2.CvtColor(crop, hsv, ColorConversionCodes.BGR2HSV);

                    int totalPixels = boxW * boxH;
                    int darkCount = 0, colorCount = 0;
                    unsafe
                    {
                        byte* p = (byte*)hsv.Data;
                        for (int px = 0; px < totalPixels; px++)
                        {
                            byte hh = p[px * 3], ss = p[px * 3 + 1], vv = p[px * 3 + 2];
                            if (vv < 95 && ss < 110) darkCount++;
                            if (ss > 80 && vv > 40) colorCount++;
                        }
                    }
                    float darkRatio = (float)darkCount / totalPixels;
                    float colorRatio = (float)colorCount / totalPixels;
                    hsv.Dispose(); crop.Dispose();

                    if (colorRatio > 0.50f && darkRatio < 0.20f) clsName = "stain";   // ★ 0.18→0.50 (정말 명백한 색일 때만 stain)
                    // ★ darkRatio < 0.05 컷 제거 - 빛 비친 구멍도 hole로 통과시킴
                }

                // ★ stain 후처리 — stain 영역이 어두우면 (= 컨베이어 색이 비치면) hole로 변경
                if (clsName == "stain" && by1 < origH && bx1 < origW && boxW > 0 && boxH > 0)
                {
                    var crop2 = new Mat(roi, new Rect(bx1, by1, boxW, boxH));
                    var hsv2 = new Mat();
                    Cv2.CvtColor(crop2, hsv2, ColorConversionCodes.BGR2HSV);

                    int totalPixels2 = boxW * boxH;
                    int darkCount2 = 0;
                    unsafe
                    {
                        byte* p = (byte*)hsv2.Data;
                        for (int px = 0; px < totalPixels2; px++)
                        {
                            byte ss = p[px * 3 + 1], vv = p[px * 3 + 2];
                            // 어두운 픽셀: V<100 AND S<100 (어두운 회색~검정 컨베이어)
                            if (vv < 100 && ss < 100) darkCount2++;
                        }
                    }
                    float darkRatio2 = (float)darkCount2 / totalPixels2;
                    hsv2.Dispose(); crop2.Dispose();

                    // 어두운 픽셀이 40% 이상 = 구멍 너머 컨베이어가 비침 → hole로 변경
                    if (darkRatio2 > 0.40f) clsName = "hole";
                }

                rawBoxes.Add(new DetectBox
                {
                    ClassName = clsName,
                    Confidence = maxScore,
                    X = ox + bx1,
                    Y = oy + by1,
                    W = boxW,
                    H = boxH
                });
            }

            // NMS per class
            rawBoxes = NmsPerClass(rawBoxes, NmsThreshold);

            // thread 최대 5개
            var threads = rawBoxes.Where(b => b.ClassName == "thread")
                                   .OrderByDescending(b => b.W * b.H).Take(5).ToList();
            var others = rawBoxes.Where(b => b.ClassName != "thread").ToList();
            return others.Concat(threads).ToList();
        }

        // ══════════════════════════════════════
        // NMS
        // ══════════════════════════════════════
        private List<DetectBox> NmsPerClass(List<DetectBox> boxes, float threshold)
        {
            var result = new List<DetectBox>();
            var groups = boxes.GroupBy(b => b.ClassName);
            foreach (var grp in groups)
            {
                var sorted = grp.OrderByDescending(b => b.Confidence).ToList();
                var keep = new bool[sorted.Count];
                Array.Fill(keep, true);

                for (int i = 0; i < sorted.Count; i++)
                {
                    if (!keep[i]) continue;
                    for (int j = i + 1; j < sorted.Count; j++)
                    {
                        if (!keep[j]) continue;
                        if (IoU(sorted[i], sorted[j]) > threshold)
                            keep[j] = false;
                    }
                }
                for (int i = 0; i < sorted.Count; i++)
                    if (keep[i]) result.Add(sorted[i]);
            }
            return result;
        }

        private float IoU(DetectBox a, DetectBox b)
        {
            int xa = Math.Max(a.X, b.X), ya = Math.Max(a.Y, b.Y);
            int xb = Math.Min(a.X + a.W, b.X + b.W), yb = Math.Min(a.Y + a.H, b.Y + b.H);
            int inter = Math.Max(0, xb - xa) * Math.Max(0, yb - ya);
            int union = a.W * a.H + b.W * b.H - inter;
            return union > 0 ? (float)inter / union : 0;
        }

        // ══════════════════════════════════════
        // Fabric 검출
        // ══════════════════════════════════════
        private (Rect? box, Mat mask) DetectFabric(Mat roi)
        {
            var gray = new Mat(); Cv2.CvtColor(roi, gray, ColorConversionCodes.BGR2GRAY);
            var blur = new Mat(); Cv2.GaussianBlur(gray, blur, new OpenCvSharp.Size(5, 5), 0);
            var bin = new Mat(); Cv2.Threshold(blur, bin, FabricBinThresh, 255, ThresholdTypes.Binary);

            var k = Cv2.GetStructuringElement(MorphShapes.Rect, new OpenCvSharp.Size(5, 5));
            Cv2.MorphologyEx(bin, bin, MorphTypes.Open, k);
            Cv2.MorphologyEx(bin, bin, MorphTypes.Close, k);

            Cv2.FindContours(bin, out var contours, out _, RetrievalModes.External, ContourApproximationModes.ApproxSimple);
            gray.Dispose(); blur.Dispose(); bin.Dispose();

            if (contours.Length == 0) return (null, null);

            var best = contours.OrderByDescending(c => Cv2.ContourArea(c)).First();
            if (Cv2.ContourArea(best) < FabricMinArea) return (null, null);

            var rect = Cv2.BoundingRect(best);
            var mask = Mat.Zeros(roi.Height, roi.Width, MatType.CV_8UC1).ToMat();
            Cv2.DrawContours(mask, new[] { best }, -1, new Scalar(255), -1);

            return (rect, mask);
        }

        // ══════════════════════════════════════
        // Rule 기반 Hole/Thread Fallback
        // ══════════════════════════════════════
        private List<DetectBox> DetectHoleThreadFallback(Mat roi, int ox, int oy)
        {
            var boxes = new List<DetectBox>();
            var (fabricBox, fabricMask) = DetectFabric(roi);
            if (fabricBox == null || fabricMask == null) return boxes;

            var fb = fabricBox.Value;
            var fabricRoi = new Mat(roi, fb);
            var fabricHsv = new Mat(); Cv2.CvtColor(fabricRoi, fabricHsv, ColorConversionCodes.BGR2HSV);
            var fabricGray = new Mat(); Cv2.CvtColor(fabricRoi, fabricGray, ColorConversionCodes.BGR2GRAY);
            var innerMask = new Mat(fabricMask, fb);

            int fabricArea = Cv2.CountNonZero(innerMask);
            if (fabricArea < 100) { fabricHsv.Dispose(); fabricGray.Dispose(); fabricMask.Dispose(); return boxes; }

            var erodeK = Cv2.GetStructuringElement(MorphShapes.Rect, new OpenCvSharp.Size(7, 7));
            var coreMask = new Mat(); Cv2.Erode(innerMask, coreMask, erodeK, iterations: 2);

            // ─── HOLE ───
            var blurGray = new Mat(); Cv2.GaussianBlur(fabricGray, blurGray, new OpenCvSharp.Size(21, 21), 0);
            var darkDiff = new Mat(); Cv2.Subtract(blurGray, fabricGray, darkDiff);

            Cv2.Split(fabricHsv, out Mat[] hsvCh);
            var dMask = new Mat(); Cv2.Compare(darkDiff, new Scalar(HoleDarkDiff), dMask, CmpTypes.GT);
            var vMask = new Mat(); Cv2.Compare(hsvCh[2], new Scalar(HoleMaxV), vMask, CmpTypes.LT);
            var sMask = new Mat(); Cv2.Compare(hsvCh[1], new Scalar(HoleMaxS), sMask, CmpTypes.LT);

            var holeMask = new Mat();
            Cv2.BitwiseAnd(dMask, vMask, holeMask);
            Cv2.BitwiseAnd(holeMask, sMask, holeMask);
            Cv2.BitwiseAnd(holeMask, coreMask, holeMask);

            var k3 = Cv2.GetStructuringElement(MorphShapes.Rect, new OpenCvSharp.Size(3, 3));
            Cv2.MorphologyEx(holeMask, holeMask, MorphTypes.Open, k3);
            Cv2.MorphologyEx(holeMask, holeMask, MorphTypes.Close, k3);

            Cv2.FindContours(holeMask, out var holeCnts, out _, RetrievalModes.External, ContourApproximationModes.ApproxSimple);
            foreach (var cnt in holeCnts)
            {
                double area = Cv2.ContourArea(cnt);
                if (area < HoleMinArea) continue;
                var r = Cv2.BoundingRect(cnt);
                float aspect = Math.Max((float)r.Width / r.Height, (float)r.Height / r.Width);
                if (aspect > HoleMaxAspect) continue;
                float fill = (float)(area / (r.Width * r.Height));
                if (fill < HoleMinFillRatio) continue;
                if (area > fabricArea * HoleMaxAreaRatio) continue;
                double peri = Cv2.ArcLength(cnt, true);
                if (peri <= 0) continue;
                double circ = 4 * Math.PI * area / (peri * peri);
                if (circ < HoleMinCircularity) continue;

                boxes.Add(new DetectBox
                {
                    ClassName = "hole",
                    Confidence = 0.90f,
                    X = ox + fb.X + r.X,
                    Y = oy + fb.Y + r.Y,
                    W = r.Width,
                    H = r.Height
                });
            }

            // ─── THREAD ───
            var grayRoi = new Mat(); Cv2.CvtColor(roi, grayRoi, ColorConversionCodes.BGR2GRAY);
            var ek = Cv2.GetStructuringElement(MorphShapes.Rect, new OpenCvSharp.Size(ThreadOuterErodeKernel, ThreadOuterErodeKernel));
            var fabricExp = new Mat(); Cv2.Dilate(fabricMask, fabricExp, ek, iterations: 1);
            var outerMask = new Mat(); Cv2.BitwiseNot(fabricExp, outerMask);

            var brightMask = new Mat(); Cv2.InRange(grayRoi, new Scalar(ThreadBrightMin), new Scalar(255), brightMask);
            Cv2.BitwiseAnd(brightMask, outerMask, brightMask);

            var thK = Cv2.GetStructuringElement(MorphShapes.Rect, new OpenCvSharp.Size(7, 7));
            var tophat = new Mat(); Cv2.MorphologyEx(grayRoi, tophat, MorphTypes.TopHat, thK);
            var topMask = new Mat(); Cv2.Threshold(tophat, topMask, 18, 255, ThresholdTypes.Binary);
            Cv2.BitwiseAnd(topMask, outerMask, topMask);

            var threadCand = new Mat(); Cv2.BitwiseOr(brightMask, topMask, threadCand);
            var k2 = Cv2.GetStructuringElement(MorphShapes.Rect, new OpenCvSharp.Size(2, 2));
            Cv2.MorphologyEx(threadCand, threadCand, MorphTypes.Open, k2);
            Cv2.MorphologyEx(threadCand, threadCand, MorphTypes.Close, k2);

            var lines = Cv2.HoughLinesP(threadCand, 1, Math.PI / 180,
                         ThreadHoughThreshold, ThreadMinLineLength, ThreadMaxLineGap);
            if (lines != null)
            {
                foreach (var seg in lines)
                {
                    int mx = (seg.P1.X + seg.P2.X) / 2, my = (seg.P1.Y + seg.P2.Y) / 2;
                    if (my >= 0 && my < fabricExp.Height && mx >= 0 && mx < fabricExp.Width)
                        if (fabricExp.At<byte>(my, mx) > 0) continue;

                    double len = Math.Sqrt(Math.Pow(seg.P2.X - seg.P1.X, 2) + Math.Pow(seg.P2.Y - seg.P1.Y, 2));
                    if (len < ThreadMinLineLength) continue;
                    if (len < 22 && (Math.Abs(seg.P2.X - seg.P1.X) < 3 || Math.Abs(seg.P2.Y - seg.P1.Y) < 3)) continue;

                    int bx = Math.Min(seg.P1.X, seg.P2.X), by = Math.Min(seg.P1.Y, seg.P2.Y);
                    int bw = Math.Max(Math.Abs(seg.P2.X - seg.P1.X), 6);
                    int bh = Math.Max(Math.Abs(seg.P2.Y - seg.P1.Y), 6);
                    if (bw * bh < 40) continue;

                    boxes.Add(new DetectBox
                    {
                        ClassName = "thread",
                        Confidence = 0.90f,
                        X = ox + bx,
                        Y = oy + by,
                        W = bw,
                        H = bh
                    });
                }
            }

            // Cleanup
            foreach (var m in new[] { fabricHsv, fabricGray, fabricMask, coreMask, blurGray, darkDiff,
                                       dMask, vMask, sMask, holeMask, grayRoi, fabricExp, outerMask,
                                       brightMask, tophat, topMask, threadCand })
                m?.Dispose();
            foreach (var c in hsvCh) c?.Dispose();

            return NmsPerClass(boxes, 0.2f);
        }
        // ══════════════════════════════════════
        // HSV Stain 검출
        // ══════════════════════════════════════
        private List<DetectBox> DetectStainHsv(Mat roi, int ox, int oy)
        {
            var boxes = new List<DetectBox>();
            var (fabricBox, fabricMask) = DetectFabric(roi);
            if (fabricBox == null || fabricMask == null) return boxes;

            var fb = fabricBox.Value;
            var fabricRoi = new Mat(roi, fb);
            var fabricHsv = new Mat(); Cv2.CvtColor(fabricRoi, fabricHsv, ColorConversionCodes.BGR2HSV);
            var innerMask = new Mat(fabricMask, fb);
            int fabricArea = Cv2.CountNonZero(innerMask);
            if (fabricArea < 100) { fabricHsv.Dispose(); fabricMask.Dispose(); return boxes; }

            var erodeK = Cv2.GetStructuringElement(MorphShapes.Rect, new OpenCvSharp.Size(9, 9));   // ★ 7→9
            var coreMask = new Mat(); Cv2.Erode(innerMask, coreMask, erodeK, iterations: 3);   // ★ 2→3

            var blurHsv = new Mat(); Cv2.GaussianBlur(fabricHsv, blurHsv, new OpenCvSharp.Size(21, 21), 0);

            Cv2.Split(fabricHsv, out Mat[] ch1);
            Cv2.Split(blurHsv, out Mat[] ch2);

            var diffH = new Mat(); Cv2.Absdiff(ch1[0], ch2[0], diffH);
            var diffS = new Mat(); Cv2.Absdiff(ch1[1], ch2[1], diffS);
            var diffV = new Mat(); Cv2.Absdiff(ch1[2], ch2[2], diffV);

            var hMask = new Mat(); Cv2.Compare(diffH, new Scalar(StainHDiff), hMask, CmpTypes.GT);
            var sMask = new Mat(); Cv2.Compare(diffS, new Scalar(StainSDiff), sMask, CmpTypes.GT);
            var vMask = new Mat(); Cv2.Compare(diffV, new Scalar(StainVDiff), vMask, CmpTypes.GT);

            var colorDiff = new Mat(); Cv2.BitwiseOr(hMask, sMask, colorDiff);
            Cv2.BitwiseOr(colorDiff, vMask, colorDiff);

            var brightExclude = new Mat(); Cv2.Compare(ch1[2], new Scalar(StainBrightExcludeV), brightExclude, CmpTypes.LE);

            var stainMask = new Mat();
            Cv2.BitwiseAnd(colorDiff, coreMask, stainMask);
            Cv2.BitwiseAnd(stainMask, brightExclude, stainMask);

            var k5 = Cv2.GetStructuringElement(MorphShapes.Rect, new OpenCvSharp.Size(5, 5));
            Cv2.MorphologyEx(stainMask, stainMask, MorphTypes.Open, k5);
            Cv2.MorphologyEx(stainMask, stainMask, MorphTypes.Close, k5);

            Cv2.FindContours(stainMask, out var cnts, out _, RetrievalModes.External, ContourApproximationModes.ApproxSimple);
            foreach (var cnt in cnts)
            {
                double area = Cv2.ContourArea(cnt);
                if (area < StainMinArea || area > fabricArea * StainMaxAreaRatio) continue;
                var r = Cv2.BoundingRect(cnt);
                boxes.Add(new DetectBox
                {
                    ClassName = "stain",
                    Confidence = 1.0f,
                    X = ox + fb.X + r.X,
                    Y = oy + fb.Y + r.Y,
                    W = r.Width,
                    H = r.Height
                });
            }

            // Cleanup
            foreach (var m in new[] { fabricHsv, fabricMask, coreMask, blurHsv, diffH, diffS, diffV,
                                       hMask, sMask, vMask, colorDiff, brightExclude, stainMask })
                m?.Dispose();
            foreach (var c in ch1) c?.Dispose();
            foreach (var c in ch2) c?.Dispose();

            return NmsPerClass(boxes, 0.2f);
        }

        public void Dispose()
        {
            session?.Dispose();
        }


    }
}