using OpenCvSharp;
using OpenCvSharp.Extensions;
using System.Diagnostics;
using System.Text;
using static Work1.DefectDetector;

namespace Work1
{
    public partial class Form1 : Form
    {
        // ==============================================================
        // ★ 설정값
        // ==============================================================
        static readonly string ONNX_MODEL = "best.onnx";
        static readonly string MONITOR_URL = "http://192.168.0.30:5000/predict";

        static readonly int CAM_WIDTH = 640;
        static readonly int CAM_HEIGHT = 480;
        static readonly int CAM_FPS = 60;

        private readonly CancellationTokenSource _cts = new CancellationTokenSource();

        // ==============================================================
        // 내부 변수
        // ==============================================================
        VideoCapture capture;
        Mat frame;
        System.Windows.Forms.Timer camTimer;
        DefectDetector detector;
        bool isProcessing = false;

        private static readonly HttpClient httpClient = new HttpClient
        {
            Timeout = TimeSpan.FromMilliseconds(500) // 응답이 늦으면 과감히 포기
        };

        // FPS 계산
        int displayFpsCount = 0;
        float displayFps = 0;
        Stopwatch displayWatch = new Stopwatch();

        int detectFpsCount = 0;
        float detectFps = 0;
        Stopwatch detectWatch = new Stopwatch();

        int totalInspected = 0;
        int totalDefect = 0;

        // 상단 변수 구역에 추가
        int streamSkipCount = 0; // 네트워크 부하를 줄이기 위해 2~3프레임당 1번 전송용

        // ★ 캐시된 검출 결과 (AnalyzeFrame이 업데이트, UpdateFrame이 읽음)
        private readonly object _cacheLock = new object();
        private List<DetectBox> _displayBoxes = new();
        private bool _displayIsDefect = false;
        private bool _displayIsNormal = false;
        private List<(string name, float conf)> _displayDefectList = new();
        private string _displayMode = "WAIT";
        private string _displayDefectRate = "";  // ★ 우하단에 표시할 불량률 문자열

        const int FABRIC_EXIT_FRAMES = 8;
        int fabricMissingCount = 0;

        FabricInfo? lockedFabricInfo = null;

        const double NEW_SAMPLE_CENTER_MOVE_PX = 45.0;
        const double NEW_SAMPLE_AREA_CHANGE_RATIO = 0.30;
        int newSampleChangeCount = 0;
        const int NEW_SAMPLE_CHANGE_FRAMES = 3;

        // ★ 배경 빼기 — 빈 컨베이어 vs 옷감 구분용 (HSV 색 임계 대신)
        const int BG_CAPTURE_FRAMES = 30;       // 시작 후 30프레임 평균을 배경으로
        const int BG_DIFF_THRESHOLD = 30;       // 배경 대비 픽셀 차이 임계
        const double CONTENT_RATIO_THRESHOLD = 0.15; // ROI 15% 이상 다르면 옷감 있음
        Mat? backgroundGray = null;
        Mat? bgAccumulator = null;
        int bgCaptureCount = 0;


        public Form1()
        {
            InitializeComponent();
        }

        private void Form1_Load(object sender, EventArgs e)
        {
            // 1) 검출기 로드
            try
            {
                string modelPath = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, ONNX_MODEL);
                if (!File.Exists(modelPath))
                {
                    MessageBox.Show(
                        $"ONNX 모델 파일이 없습니다!\n경로: {modelPath}\n\n" +
                        "1) python convert_to_onnx.py 실행\n" +
                        "2) best.onnx를 bin/Debug에 복사",
                        "모델 없음", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return;
                }

                detector = new DefectDetector(modelPath);
                this.Text = "Fabric Defect Detector — 모델 로드 완료";
            }
            catch (Exception ex)
            {
                MessageBox.Show($"모델 로드 실패: {ex.Message}");
                return;
            }

            // 2) 카메라
            capture = new VideoCapture(0, VideoCaptureAPIs.DSHOW);
            if (capture.IsOpened())
            {
                UpdateConnectionUI(true, "CONNECTED"); // 카메라 연결 성공
            }
            else
            {
                UpdateConnectionUI(false, "DISCONNECTED"); // 카메라 연결 실패
                MessageBox.Show("카메라 미연결");
                return;
            }

            capture.FrameWidth = CAM_WIDTH;
            capture.FrameHeight = CAM_HEIGHT;
            frame = new Mat();

            // 3) 타이머
            camTimer = new System.Windows.Forms.Timer();
            camTimer.Interval = 1000 / CAM_FPS;
            camTimer.Tick += UpdateFrame;
            camTimer.Start();

            displayWatch.Start();
            detectWatch.Start();
        }

        private void Form1_FormClosing(object sender, FormClosingEventArgs e)
        {
            _cts.Cancel(); // 진행 중인 모든 통신 중단
            camTimer?.Stop();
            capture?.Release();
            frame?.Dispose();
            detector?.Dispose();
            backgroundGray?.Dispose();
            bgAccumulator?.Dispose();
        }

        // ==============================================================
        // ★ 프레임 갱신 — 항상 30fps로 화면 갱신
        // 검출 결과는 캐시에서 읽어서 오버레이
        // ==============================================================
        private void UpdateFrame(object sender, EventArgs e)
        {
            if (capture == null || !capture.IsOpened() || detector == null) return;

            capture.Read(frame);
            if (frame.Empty()) return;

            // 1. 캐시 데이터 읽기
            List<DetectBox> boxes;
            bool isDefect;
            bool isNormal;
            List<(string name, float conf)> defectList;
            string mode;
            string defectRate;

            lock (_cacheLock)
            {
                boxes = _displayBoxes;
                isDefect = _displayIsDefect;
                isNormal = _displayIsNormal;
                defectList = _displayDefectList;
                mode = _displayMode;
                defectRate = _displayDefectRate;
            }

            // 2. FPS 계산
            displayFpsCount++;
            if (displayWatch.ElapsedMilliseconds >= 1000)
            {
                displayFps = displayFpsCount * 1000f / displayWatch.ElapsedMilliseconds;
                displayFpsCount = 0;
                displayWatch.Restart();
            }

            try
            {
                // 3. 화면 그리기 (딱 한 번만 실행)
                using var annotated = detector.DrawAnnotated(frame, boxes, isDefect, isNormal, defectList, mode, defectRate);

                // UI 업데이트
                var oldImg = picCamera.Image;
                picCamera.Image = BitmapConverter.ToBitmap(annotated);
                oldImg?.Dispose();

                // 4. Flask 전송 (이미 그려진 annotated 이미지를 그대로 활용)
                streamSkipCount++;
                if (streamSkipCount >= 3) // 약 20fps로 전송
                {
                    streamSkipCount = 0;
                    // 이미지를 복제하여 비동기로 전송 (분석 루프 보호)
                    Mat sendFrame = annotated.Clone();
                    _ = Task.Run(() => SendLiveStreamOptimized(sendFrame, isDefect, mode));
                }

                SetStatus(isDefect, isNormal);
                this.Text = $"Fabric Defect — 화면:{displayFps:F0}fps 검출:{detectFps:F0}fps 검사:{totalInspected} 불량:{totalDefect}";
            }
            catch { }

            // 5. 검출 분석 (백그라운드)
            if (!isProcessing)
            {
                Mat frameCopy = frame.Clone();
                _ = Task.Run(() => AnalyzeFrame(frameCopy));
            }
        }
        private async Task SendLiveStreamOptimized(Mat mat, bool isDefect, string mode)
        {
            try
            {
                // 1. 이미지 인코딩 품질을 낮춰 전송 속도 극대화 (동기화의 핵심)
                byte[] jpg = mat.ToBytes(".jpg", new int[] { (int)ImwriteFlags.JpegQuality, 70 });
                string b64 = Convert.ToBase64String(jpg);

                var payload = new
                {
                    image_data = b64,
                    is_defective = isDefect,
                    status = mode,
                    timestamp = DateTime.Now.ToString("HH:mm:ss.fff")
                };

                string json = System.Text.Json.JsonSerializer.Serialize(payload);
                var content = new StringContent(json, Encoding.UTF8, "application/json");

                // 2. 응답을 기다리되, 분석 루프와 분리되어 있으므로 전체 FPS에는 영향 없음
                var response = await httpClient.PostAsync(MONITOR_URL, content, _cts.Token);
                if (response.IsSuccessStatusCode)
                {
                    // 통신 성공 시 상태 유지 (너무 자주 호출되면 UI 부하가 있으니 상태 확인 후 호출 권장)
                    if (lblConnStatus.Text != "CONNECTED") UpdateConnectionUI(true, "CONNECTED");
                }
                response.Dispose();
            }
            catch (TaskCanceledException)
            {
                // 타임아웃 또는 폼 종료 시 정상적으로 발생 — 무시
            }
            catch (OperationCanceledException)
            {
                // 폼 종료 시 _cts.Cancel()로 발생 — 무시
            }
            catch
            {
                UpdateConnectionUI(false, "SERVER ERROR");
            }
            finally
            {
                mat.Dispose(); // 복제했던 이미지 메모리 해제
            }
        }

        // ==============================================================
        // ★ 분석 — 백그라운드에서 실행, 결과를 캐시에 저장
        // 화면 갱신과 완전히 분리됨
        // ==============================================================

        int defectConfirmCount = 0;      // 연속 검출 카운트
        const int CONFIRM_THRESHOLD = 5; // 7프레임 연속 시 확정
        bool isSampleLocked = false;     // 현재 샘플 전송 완료 여부 (중복 방지)

        // ★ 락 후 결과 표시 유지용 카운터
        //   불량/정상 확정 직후 N프레임 동안 박스 + 상태(OK/DEFECT)를 화면에 유지
        //   분석 fps ~15라 가정하면 30프레임 = 약 2초
        const int POST_LOCK_KEEP_FRAMES = 30;
        int postLockFrameCount = 0;

        const int NORMAL_CHECK_FRAMES = 15;
        const int STABLE_THRESHOLD = 8;   // ★ 5 → 8 (확실히 정지한 후에만 검사 시작)
        const double MOTION_DIFF_THRESHOLD = 15.0;   // ★ 8 → 15 (작은 픽셀 변화는 노이즈로 무시)
        const double MOTION_RATIO_THRESHOLD = 0.04;  // ★ 0.015 → 0.04 (미세 흔들림 허용)

        int stableFabricCount = 0;
        Mat? prevStableGray = null;
        int normalCheckCount = 0;
        bool sampleHadAnyDefect = false;
        FabricInfo? prevFabricInfo = null;

        private void AnalyzeFrame(Mat mat)
        {
            isProcessing = true;

            try
            {
                // ★ 배경 캡처 (시작 후 30프레임 평균)
                if (backgroundGray == null)
                {
                    AccumulateBackground(mat);
                    lock (_cacheLock)
                    {
                        _displayMode = $"BG CAPTURE {bgCaptureCount}/{BG_CAPTURE_FRAMES}";
                        _displayBoxes = new List<DetectBox>();
                        _displayIsDefect = false;
                        _displayIsNormal = false;
                    }
                    return;
                }

                // 1. ROI 안에 실제 옷감이 있는지 확인 (배경 빼기 방식)
                var fabricInfo = detector.DetectFabricInRoi(mat);
                bool hasContent = HasContentInRoi(mat);

                // ★ HSV로 옷감 인식한 결과는 무시하고, 배경 빼기 결과를 사용
                fabricInfo.IsPresent = hasContent;

                // ★ 디버그: 두 판정 결과 모두 표시
                string dbgInfo = $"[bg={hasContent} hsv_area={fabricInfo.Area:F0}]";

                // ★★★ 락 걸린 상태에서는 옷감이 빠지는 것만 감시 ★★★
                if (isSampleLocked)
                {
                    if (!fabricInfo.IsPresent)
                    {
                        // 옷감 빠짐 카운트 증가 (연속 안 끊김 — 한번 빠지기 시작하면 누적)
                        fabricMissingCount++;
                    }
                    // ★ 옷감이 다시 들어왔다고 fabricMissingCount를 0으로 리셋 안 함
                    //   = 옷감 A가 잠깐 빠지고 옷감 B가 곧바로 들어와도 누적 카운트로 락 풀림

                    if (fabricMissingCount >= FABRIC_EXIT_FRAMES)
                    {
                        ResetForNextSample();
                        // 락 풀렸으니 이번 프레임은 다음 샘플 검사 시작 가능 — return 안 하고 진행
                    }
                    else
                    {
                        // ★ 락 직후 N프레임은 결과 박스/상태 그대로 유지 → 사용자가 결과 볼 시간 확보
                        //   N프레임 지난 후엔 박스만 클리어 (다음 샘플 대기 표시)
                        postLockFrameCount++;

                        lock (_cacheLock)
                        {
                            if (postLockFrameCount >= POST_LOCK_KEEP_FRAMES)
                            {
                                // 유지 시간 끝 — 박스 잔상 제거
                                _displayBoxes = new List<DetectBox>();
                                _displayDefectList = new List<(string name, float conf)>();
                                _displayIsDefect = false;
                                _displayIsNormal = false;
                            }
                            // 그 전엔 _displayBoxes / _displayIsDefect / _displayIsNormal 그대로 둠
                            _displayMode = $"WAIT SAMPLE OUT {fabricMissingCount}/{FABRIC_EXIT_FRAMES} {dbgInfo}";
                        }
                        return;
                    }
                }

                // 락 안 걸린 상태에서 옷감 없으면 대기
                if (!fabricInfo.IsPresent)
                {
                    fabricMissingCount++;

                    defectConfirmCount = 0;
                    stableFabricCount = 0;
                    normalCheckCount = 0;
                    sampleHadAnyDefect = false;
                    prevStableGray?.Dispose();
                    prevStableGray = null;

                    lock (_cacheLock)
                    {
                        _displayBoxes = new List<DetectBox>();
                        _displayIsDefect = false;
                        _displayIsNormal = false;   // ★ OK 상태도 꺼짐 (옷감 빠지면 즉시)
                        _displayDefectList = new List<(string name, float conf)>();
                        _displayMode = $"NO FABRIC {fabricMissingCount} {dbgInfo}";
                    }

                    return;
                }
                else
                {
                    // 락 안 걸리고 옷감 있을 때만 카운터 리셋
                    fabricMissingCount = 0;
                }

                // 2. 옷감이 정지했는지 판단
                bool isStable = IsFabricStableByFrameDiff(mat);

                stableFabricCount = isStable ? stableFabricCount + 1 : 0;
                bool fabricStopped = stableFabricCount >= STABLE_THRESHOLD;

                // 3. 불량 검사
                var (boxes, rawIsDefect, rawIsNormal, defectList, mode) = detector.ProcessFrame(mat);

                bool finalIsDefect = false;
                bool finalisNormal = false;

                if (!fabricStopped)
                {
                    defectConfirmCount = 0;
                    normalCheckCount = 0;
                    sampleHadAnyDefect = false;
                    mode = "FABRIC MOVING";
                }
                else if (!isSampleLocked && fabricInfo.IsPresent)  // ★ 옷감이 실제로 있을 때만 검사
                {
                    if (rawIsDefect)
                    {
                        sampleHadAnyDefect = true;
                        defectConfirmCount++;
                        normalCheckCount = 0;
                    }
                    else
                    {
                        defectConfirmCount = 0;

                        if (!sampleHadAnyDefect)
                        {

                            normalCheckCount++;
                            mode = $"NORMAL CHECK {normalCheckCount}/{NORMAL_CHECK_FRAMES} {dbgInfo}";
                        }
                    }

                    if (defectConfirmCount >= CONFIRM_THRESHOLD)
                    {
                        finalIsDefect = true;
                    }
                }
                else if (!fabricInfo.IsPresent)
                {
                    // 정지했지만 옷감 없음 → 카운터 리셋, 검사 진입 안 함
                    defectConfirmCount = 0;
                    normalCheckCount = 0;
                    sampleHadAnyDefect = false;
                    mode = "NO FABRIC";
                }
                else
                {
                    mode = "WAIT SAMPLE OUT";
                }
                // 4. 화면 표시용 캐시 갱신
                //    ★ 정지 상태일 때만 박스/검출 결과 표시 (움직이는 중엔 빈 박스)
                lock (_cacheLock)
                {
                    if (fabricStopped && fabricInfo.IsPresent)
                    {
                        _displayBoxes = boxes;
                        _displayDefectList = defectList;
                    }
                    else
                    {
                        _displayBoxes = new List<DetectBox>();
                        _displayDefectList = new List<(string name, float conf)>();
                    }
                    _displayIsDefect = finalIsDefect;
                    _displayIsNormal = finalisNormal;
                    _displayMode = mode;
                }

                // 5. 불량 확정
                if (finalIsDefect && !isSampleLocked)
                {
                    isSampleLocked = true;
                    postLockFrameCount = 0;   // ★ 락 직후부터 카운트 시작 (박스 N프레임 유지)
                    lockedFabricInfo = fabricInfo;
                    totalDefect++;
                    totalInspected++;

                    // ★ 불량률 백그라운드 계산 (분석 루프에 영향 없음)
                    UpdateDefectRateAsync();

                    Mat defectCapture = mat.Clone();

                    _ = SendDefectToMonitor(mat, boxes, defectList, mode);
                    _ = SendDefectToFlask(defectList);
                    _ = SendHighQualityCapture(defectCapture, defectList);

                    string defectTypes = string.Join(", ", defectList.Select(d => d.name).Distinct());
                    int defectCount = defectList.Count;

                    SetStatus(true, false);
                    AddLogToGrid(defectTypes, defectCount.ToString() + "개", true);
                    //AppendLog(defectTypes, defectCount);

                    Debug.WriteLine("🚨 불량 샘플 확정");
                }
                // 6. 정상 확정
                else if (fabricStopped && fabricInfo.IsPresent && !isSampleLocked && !sampleHadAnyDefect && normalCheckCount >= NORMAL_CHECK_FRAMES)
                {
                    isSampleLocked = true;
                    postLockFrameCount = 0;   // ★ 락 직후부터 카운트 시작 (OK 상태 N프레임 유지)
                    lockedFabricInfo = fabricInfo;
                    totalInspected++;

                    // ★ 불량률 백그라운드 계산 (분석 루프에 영향 없음)
                    UpdateDefectRateAsync();

                    SetStatus(false, true);
                    _ = SendNormalToFlask();
                    AddLogToGrid("OK", "-", false);
                    //AppendNormalLog();

                    lock (_cacheLock)
                    {
                        _displayIsNormal = true;
                        _displayMode = "NORMAL CONFIRMED";
                    }

                    Debug.WriteLine("✅ 정상 샘플 확정");
                }

                detectFpsCount++;

                if (detectWatch.ElapsedMilliseconds >= 1000)
                {
                    detectFps = detectFpsCount * 1000f / detectWatch.ElapsedMilliseconds;
                    detectFpsCount = 0;
                    detectWatch.Restart();
                }
            }
            catch (Exception ex)
            {
                Debug.WriteLine($"분석 에러: {ex.Message}");
            }
            finally
            {
                mat.Dispose();
                isProcessing = false;
            }
        }

        // ==============================================================
        // ★ 불량률 계산 — 백그라운드 스레드에서 실행 (분석 루프 보호)
        //   호출 시점: 불량 또는 정상 확정 직후
        //   작업 자체는 가벼우나(나눗셈 1번) 안전 위해 Task로 분리
        // ==============================================================
        private void UpdateDefectRateAsync()
        {
            // 카운트 스냅샷을 먼저 찍어서 Task에 넘김 (race condition 방지)
            int total = totalInspected;
            int defect = totalDefect;

            _ = Task.Run(() =>
            {
                float rate = total > 0 ? (defect * 100f / total) : 0f;
                string rateStr = $"Defect Rate: {rate:F1}% ({defect}/{total})";

                lock (_cacheLock)
                {
                    _displayDefectRate = rateStr;
                }
            });
        }

        private async Task SendHighQualityCapture(Mat mat, List<(string name, float conf)> defectList)
        {
            try
            {
                // 1. 선명화 처리 (필요 시) 및 고품질 인코딩 (품질 95 이상)
                // DrawAnnotated를 사용하여 박스가 그려진 이미지를 보낼 수도 있고, 생 이미지를 보낼 수도 있습니다.
                using var annotated = detector.DrawAnnotated(mat, _displayBoxes, true, false, defectList, "DEFECT_DETECTED", _displayDefectRate);
                byte[] jpgData = annotated.ToBytes(".jpg", new int[] { (int)ImwriteFlags.JpegQuality, 95 });
                string b64Image = Convert.ToBase64String(jpgData);

                // 2. 데이터 구성
                var payload = new
                {
                    event_type = "DEFECT_CAPTURE", // 일반 스트림과 구분용
                    timestamp = DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss.fff"),
                    defect_details = defectList.Select(d => new { type = d.name, conf = d.conf }),
                    image_data = b64Image
                };

                string json = System.Text.Json.JsonSerializer.Serialize(payload);
                var content = new StringContent(json, Encoding.UTF8, "application/json");

                // 3. Flask 서버의 특정 경로로 전송 (예: /api/capture)
                // MONITOR_URL 대신 캡처 전용 URL이 있다면 그것을 사용하세요.
                string captureUrl = MONITOR_URL.Replace("/predict", "/api/capture");
                await httpClient.PostAsync(captureUrl, content);

                Debug.WriteLine("📸 고화질 결함 캡처 전송 완료");
            }
            catch (Exception ex)
            {
                Debug.WriteLine($"❌ 캡처 전송 실패: {ex.Message}");
            }
            finally
            {
                mat.Dispose(); // 복사했던 이미지 메모리 해제
            }
        }
        private void ResetForNextSample()
        {
            isSampleLocked = false;
            postLockFrameCount = 0;   // ★ 락 후 유지 카운터도 리셋

            defectConfirmCount = 0;
            stableFabricCount = 0;
            normalCheckCount = 0;
            sampleHadAnyDefect = false;
            fabricMissingCount = 0;
            newSampleChangeCount = 0;

            lockedFabricInfo = null;

            prevStableGray?.Dispose();
            prevStableGray = null;

            detector.ResetState();

            lock (_cacheLock)
            {
                _displayBoxes = new List<DetectBox>();
                _displayIsDefect = false;
                _displayIsNormal = false;   // ★ OK 상태도 리셋
                _displayDefectList = new List<(string name, float conf)>();
                _displayMode = "RESET";
            }
        }

        // ==============================================================
        // ★ 배경 누적 (시작 후 30프레임 평균을 빈 컨베이어 기준으로 저장)
        // ==============================================================
        private void AccumulateBackground(Mat mat)
        {
            var (rx1, ry1, rx2, ry2) = detector.GetCenterRoi(mat);
            using var roi = new Mat(mat, new Rect(rx1, ry1, rx2 - rx1, ry2 - ry1));
            using var gray = new Mat();
            Cv2.CvtColor(roi, gray, ColorConversionCodes.BGR2GRAY);
            Cv2.GaussianBlur(gray, gray, new OpenCvSharp.Size(7, 7), 0);

            if (bgAccumulator == null)
            {
                bgAccumulator = new Mat();
                gray.ConvertTo(bgAccumulator, MatType.CV_32FC1);
            }
            else
            {
                using var grayF = new Mat();
                gray.ConvertTo(grayF, MatType.CV_32FC1);
                Cv2.Add(bgAccumulator, grayF, bgAccumulator);
            }

            bgCaptureCount++;

            if (bgCaptureCount >= BG_CAPTURE_FRAMES)
            {
                using var avg = new Mat();
                Cv2.Divide(bgAccumulator, new Scalar(bgCaptureCount), avg);
                backgroundGray = new Mat();
                avg.ConvertTo(backgroundGray, MatType.CV_8UC1);

                bgAccumulator.Dispose();
                bgAccumulator = null;

                Debug.WriteLine($"✅ 배경 캡처 완료 ({BG_CAPTURE_FRAMES}프레임 평균)");
            }
        }

        // ==============================================================
        // ★ ROI에 옷감(=배경과 다른 무엇)이 있는가? — 배경 빼기 방식
        //   HSV 색 임계가 아닌 "기준 대비 변화"로 판정 → 컨베이어 색 무관
        // ==============================================================
        private bool HasContentInRoi(Mat mat)
        {
            if (backgroundGray == null) return false;

            var (rx1, ry1, rx2, ry2) = detector.GetCenterRoi(mat);
            using var roi = new Mat(mat, new Rect(rx1, ry1, rx2 - rx1, ry2 - ry1));
            using var gray = new Mat();
            Cv2.CvtColor(roi, gray, ColorConversionCodes.BGR2GRAY);
            Cv2.GaussianBlur(gray, gray, new OpenCvSharp.Size(7, 7), 0);

            using var diff = new Mat();
            Cv2.Absdiff(backgroundGray, gray, diff);

            using var contentMask = new Mat();
            Cv2.Threshold(diff, contentMask, BG_DIFF_THRESHOLD, 255, ThresholdTypes.Binary);

            using var k = Cv2.GetStructuringElement(MorphShapes.Rect,
                                                     new OpenCvSharp.Size(5, 5));
            Cv2.MorphologyEx(contentMask, contentMask, MorphTypes.Open, k);

            double contentPixels = Cv2.CountNonZero(contentMask);
            double totalPixels = Math.Max(1, contentMask.Width * contentMask.Height);
            double contentRatio = contentPixels / totalPixels;

            return contentRatio > CONTENT_RATIO_THRESHOLD;
        }

        private bool IsFabricStableByFrameDiff(Mat mat)
        {
            var (rx1, ry1, rx2, ry2) = detector.GetCenterRoi(mat);

            using var roi = new Mat(mat, new Rect(rx1, ry1, rx2 - rx1, ry2 - ry1));
            using var gray = new Mat();

            Cv2.CvtColor(roi, gray, ColorConversionCodes.BGR2GRAY);
            Cv2.GaussianBlur(gray, gray, new OpenCvSharp.Size(5, 5), 0);

            if (prevStableGray == null || prevStableGray.Empty())
            {
                prevStableGray?.Dispose();
                prevStableGray = gray.Clone();
                return false;
            }

            using var diff = new Mat();
            Cv2.Absdiff(prevStableGray, gray, diff);

            using var motionMask = new Mat();
            Cv2.Threshold(diff, motionMask, MOTION_DIFF_THRESHOLD, 255, ThresholdTypes.Binary);

            double motionPixels = Cv2.CountNonZero(motionMask);
            double totalPixels = Math.Max(1, motionMask.Width * motionMask.Height);
            double motionRatio = motionPixels / totalPixels;

            prevStableGray.Dispose();
            prevStableGray = gray.Clone();

            return motionRatio < MOTION_RATIO_THRESHOLD;
        }

        private async Task SendNormalToFlask()
        {
            try
            {
                var payload = new
                {
                    timestamp = DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss")
                };

                string json = System.Text.Json.JsonSerializer.Serialize(payload);
                var content = new StringContent(json, Encoding.UTF8, "application/json");

                string flaskUrl = MONITOR_URL.Replace(
                    "http://192.168.0.30:5000/predict",
                    "http://192.168.0.30:5000/api/defect"
                );

                await httpClient.PostAsync(flaskUrl, content);

                Debug.WriteLine("✅ 정상 샘플 전송 완료");
            }
            catch (Exception ex)
            {
                Debug.WriteLine($"❌ 정상 샘플 전송 실패: {ex.Message}");
            }
        }

        private async Task SendDefectToMonitor(Mat mat, List<DetectBox> boxes, List<(string name, float conf)> defectList, string mode)
        {
            if (string.IsNullOrEmpty(MONITOR_URL)) return;

            try
            {
                // 확정된 순간의 이미지 생성
                using var annotated = detector.DrawAnnotated(mat, boxes, true, false, defectList, mode, _displayDefectRate);
                byte[] jpg = annotated.ToBytes(".jpg", new int[] { (int)ImwriteFlags.JpegQuality, 75 });
                string b64 = Convert.ToBase64String(jpg);

                var payload = new
                {
                    status = "DEFECT",
                    device_id = "WINFORM_CAM_01",
                    timestamp = DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss.fff"),
                    data = new
                    {
                        is_defective = true,
                        image = b64,
                        details = defectList.Select(d => new { type = d.name, conf = d.conf }).ToList()
                    }
                };

                string json = System.Text.Json.JsonSerializer.Serialize(payload);
                var content = new StringContent(json, Encoding.UTF8, "application/json");
                await httpClient.PostAsync(MONITOR_URL, content);
            }
            catch (Exception ex) { Debug.WriteLine($"전송 실패: {ex.Message}"); }
        }

        // ==============================================================
        // ★ Flask /api/defect 로 불량 데이터 전송 → MySQL DB 저장
        //   전송 데이터: timestamp(결함 시간), defect_type(결함 유형), defect_count(결함 개수)
        // ==============================================================
        private async Task SendDefectToFlask(List<(string name, float conf)> defectList)
        {
            try
            {
                string defectTypes = string.Join(", ", defectList.Select(d => d.name).Distinct());
                int defectCount = defectList.Count;

                var payload = new
                {
                    timestamp = DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss"),
                    defect_type = defectTypes,
                    defect_count = defectCount
                };

                string json = System.Text.Json.JsonSerializer.Serialize(payload);
                var content = new StringContent(json, Encoding.UTF8, "application/json");

                // Flask 서버의 /api/defect 엔드포인트로 POST
                string flaskUrl = MONITOR_URL.Replace("http://192.168.0.30:5000/predict", "http://192.168.0.30:5000/api/defect");
                await httpClient.PostAsync(flaskUrl, content);

                Debug.WriteLine($"✅ [Flask DB] 불량 전송 완료 — 유형: {defectTypes}, 개수: {defectCount}");
            }
            catch (Exception ex)
            {
                Debug.WriteLine($"❌ [Flask DB] 전송 실패: {ex.Message}");
            }
        }


        // ==============================================================
        // 상태 표시등
        // ==============================================================
        private void SetStatus(bool isDefective, bool isNormal)
        {
            // UI 컨트롤 접근을 위한 Invoke 확인 (스레드 안전)
            if (this.InvokeRequired)
            {
                this.Invoke(new Action(() => SetStatus(isDefective, isNormal)));
                return;
            }

            if (isDefective)
            {
                // 1. 불량 검출 시
                lblStatusText.Text = "DEFECT";
                lblStatusText.ForeColor = Color.Red;
            }
            else if (isNormal)
            {
                // 2. 정상 확정 시
                lblStatusText.Text = "OK";
                lblStatusText.ForeColor = Color.LimeGreen;
            }
            else
            {
                // 3. 샘플이 없거나 분석 중인 대기 상태
                lblStatusText.Text = "WAIT";
                lblStatusText.ForeColor = Color.Gray;
            }
        }
        private void AddLogToGrid(string type, string count, bool isDefect)
        {
            if (dgv_Log.InvokeRequired)
            {
                dgv_Log.Invoke(new Action(() => AddLogToGrid(type, count, isDefect)));
                return;
            }

            // ★ 핵심: 변수에 대입하지 않고 바로 Insert 호출 (오류 방지)
            dgv_Log.Rows.Insert(0, DateTime.Now.ToString("HH:mm:ss"), type, count);

            // 첫 번째 행(방금 추가한 행)의 스타일 설정
            DataGridViewRow row = dgv_Log.Rows[0];

            if (isDefect)
            {
                // 불량 종류에 따른 상세 색상 설정
                Color defectColor;

                // 쉼표(,)가 포함된 "Thread,Hole" 같은 복합 불량도 처리하기 위해 분기
                if (type.Contains("Thread") && type.Contains("Hole"))
                {
                    defectColor = Color.MediumPurple; // 복합 불량은 보라색 계열 (예시)
                }
                else
                {
                    switch (type)
                    {
                        case "Thread":
                            defectColor = Color.FromArgb(255, 177, 66); // 주황색
                            break;
                        case "Hole":
                            defectColor = Color.FromArgb(255, 82, 82);  // 빨간색
                            break;
                        case "Stain":
                            defectColor = Color.LightSkyBlue;           // 하늘색 (예시)
                            break;
                        default:
                            defectColor = Color.HotPink;                // 정의되지 않은 기타 불량
                            break;
                    }
                }

                // 선택된 색상을 셀에 적용
                row.Cells[1].Style.ForeColor = defectColor; // 종류 열
                row.Cells[2].Style.ForeColor = defectColor; // 갯수 열
            }
            else
            {
                // 정상일 때 (normal 등)
                row.Cells[1].Style.ForeColor = Color.SpringGreen;
                row.Cells[2].Style.ForeColor = Color.SpringGreen;
            }
        }


        private void UpdateConnectionUI(bool isConnected, string message)
        {
            if (this.InvokeRequired)
            {
                this.Invoke(new Action(() => UpdateConnectionUI(isConnected, message)));
                return;
            }

            lblConnStatus.Text = message;

            if (isConnected)
            {
                pnlStatusDot.BackColor = Color.SpringGreen;
            }
            else
            {
                pnlStatusDot.BackColor = Color.OrangeRed;
            }
        }
    }
}