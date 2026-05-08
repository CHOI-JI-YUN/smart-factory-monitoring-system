using OpenCvSharp;
using OpenCvSharp.Extensions;
using System.Diagnostics;

namespace Work1
{
    /// <summary>
    /// Stain + Hole 검출 검증용 도구 (재검증판)
    /// - 메인 코드 현재 설정과 동일한 초기값으로 시작
    /// - 슬라이더로 미세조정 후 메인 코드에 반영
    /// </summary>
    public partial class Form2 : Form
    {
        VideoCapture capture;
        Mat frame;
        System.Windows.Forms.Timer camTimer;

        // UI
        PictureBox picOriginal;
        PictureBox picFabricMask;
        PictureBox picStainMask;
        PictureBox picHoleAnalysis;
        Label lblStatus;
        TrackBar tbHDiff, tbSDiff, tbVDiff, tbMinArea, tbBrightExclude;
        TrackBar tbErodeKernel, tbErodeIter;
        Label lblHDiff, lblSDiff, lblVDiff, lblMinArea, lblBrightExclude;
        Label lblErodeKernel, lblErodeIter;

        // ★ 메인 코드와 동일한 검증된 초기값
        int StainHDiff = 8;
        int StainSDiff = 100;
        int StainVDiff = 46;
        int StainMinArea = 200;
        int StainBrightExcludeV = 242;
        int FabricBinThresh = 80;
        int FabricMinArea = 5000;     // ★ 메인과 동일

        // ★ 침식 파라미터 (메인 DetectStainHsv와 동일)
        int ErodeKernelSize = 9;
        int ErodeIterations = 3;

        // ★ ROI 비율 — 메인 코드의 RoiWRatio/RoiHRatio = 0.65f와 동일
        const float RoiWRatio = 0.65f;
        const float RoiHRatio = 0.65f;

        public Form2()
        {
            this.Text = "Stain + Hole 검증 도구 (메인 설정 동기화)";
            this.Size = new System.Drawing.Size(1450, 880);
            this.BackColor = System.Drawing.Color.FromArgb(30, 30, 30);

            BuildUI();

            capture = new VideoCapture(0, VideoCaptureAPIs.DSHOW);
            if (!capture.IsOpened())
            {
                MessageBox.Show("카메라 열기 실패");
                return;
            }
            capture.FrameWidth = 640;
            capture.FrameHeight = 480;
            frame = new Mat();

            camTimer = new System.Windows.Forms.Timer { Interval = 33 };
            camTimer.Tick += UpdateFrame;
            camTimer.Start();
        }

        private void BuildUI()
        {
            // 원본
            this.Controls.Add(MakeLabel("[1] Original + Detection", 10, 10, 400, true));
            picOriginal = MakePB(10, 35, 640, 480);
            this.Controls.Add(picOriginal);

            // 옷감 마스크
            this.Controls.Add(MakeLabel("[2] Fabric Mask (white = fabric)", 670, 10, 300, true));
            picFabricMask = MakePB(670, 35, 300, 175);
            this.Controls.Add(picFabricMask);

            // Stain 마스크
            this.Controls.Add(MakeLabel("[3] Stain Mask", 670, 220, 300, true));
            picStainMask = MakePB(670, 245, 300, 165);
            this.Controls.Add(picStainMask);

            // Hole 영역 분석
            this.Controls.Add(MakeLabel("[4] Hole Region (dark=red, bright=blue)", 670, 420, 350, true));
            picHoleAnalysis = MakePB(670, 445, 300, 165);
            this.Controls.Add(picHoleAnalysis);

            // 상태
            lblStatus = new Label
            {
                Location = new System.Drawing.Point(10, 525),
                Size = new System.Drawing.Size(640, 280),
                ForeColor = System.Drawing.Color.LimeGreen,
                BackColor = System.Drawing.Color.Black,
                Font = new System.Drawing.Font("Consolas", 10),
                BorderStyle = BorderStyle.FixedSingle,
                Text = "Status: Starting..."
            };
            this.Controls.Add(lblStatus);

            // 슬라이더
            int sx = 990, sy = 35;
            int dy = 55;

            (tbHDiff, lblHDiff) = AddSlider(sx, sy + dy * 0, "H Diff Threshold", StainHDiff, 5, 50,
                v => StainHDiff = v);
            (tbSDiff, lblSDiff) = AddSlider(sx, sy + dy * 1, "S Diff Threshold", StainSDiff, 5, 150,
                v => StainSDiff = v);
            (tbVDiff, lblVDiff) = AddSlider(sx, sy + dy * 2, "V Diff Threshold", StainVDiff, 5, 100,
                v => StainVDiff = v);
            (tbMinArea, lblMinArea) = AddSlider(sx, sy + dy * 3, "Stain Min Area (px)", StainMinArea, 10, 800,
                v => StainMinArea = v);
            (tbBrightExclude, lblBrightExclude) = AddSlider(sx, sy + dy * 4, "Bright Exclude V", StainBrightExcludeV, 150, 255,
                v => StainBrightExcludeV = v);

            (tbErodeKernel, lblErodeKernel) = AddSlider(sx, sy + dy * 5, "Erode Kernel (odd)", ErodeKernelSize, 3, 21,
                v =>
                {
                    if (v % 2 == 0) v++;
                    ErodeKernelSize = v;
                });
            (tbErodeIter, lblErodeIter) = AddSlider(sx, sy + dy * 6, "Erode Iterations", ErodeIterations, 1, 8,
                v => ErodeIterations = v);

            var btnReset = new Button
            {
                Text = "Reset to Main Defaults (8/100/46/200, 9x9 x3)",
                Location = new System.Drawing.Point(sx, sy + dy * 7 + 10),
                Size = new System.Drawing.Size(380, 30),
                BackColor = System.Drawing.Color.FromArgb(60, 60, 60),
                ForeColor = System.Drawing.Color.White
            };
            btnReset.Click += (s, e) =>
            {
                tbHDiff.Value = 8;
                tbSDiff.Value = 100;
                tbVDiff.Value = 46;
                tbMinArea.Value = 200;
                tbBrightExclude.Value = 242;
                tbErodeKernel.Value = 9;
                tbErodeIter.Value = 3;
            };
            this.Controls.Add(btnReset);

            this.FormClosing += (s, e) =>
            {
                camTimer?.Stop();
                capture?.Release();
                frame?.Dispose();
            };
        }

        private Label MakeLabel(string text, int x, int y, int w, bool bold)
        {
            return new Label
            {
                Text = text,
                ForeColor = System.Drawing.Color.White,
                Location = new System.Drawing.Point(x, y),
                Size = new System.Drawing.Size(w, 20),
                Font = new System.Drawing.Font("Segoe UI", 10,
                    bold ? System.Drawing.FontStyle.Bold : System.Drawing.FontStyle.Regular)
            };
        }

        private PictureBox MakePB(int x, int y, int w, int h)
        {
            return new PictureBox
            {
                Location = new System.Drawing.Point(x, y),
                Size = new System.Drawing.Size(w, h),
                BorderStyle = BorderStyle.FixedSingle,
                SizeMode = PictureBoxSizeMode.StretchImage
            };
        }

        private (TrackBar, Label) AddSlider(int x, int y, string title, int defaultVal, int min, int max, Action<int> onChange)
        {
            var lbl = new Label
            {
                Location = new System.Drawing.Point(x, y),
                Size = new System.Drawing.Size(380, 18),
                ForeColor = System.Drawing.Color.White,
                Text = $"{title}: {defaultVal}"
            };
            this.Controls.Add(lbl);

            var tb = new TrackBar
            {
                Location = new System.Drawing.Point(x, y + 18),
                Size = new System.Drawing.Size(380, 30),
                Minimum = min,
                Maximum = max,
                Value = defaultVal,
                TickFrequency = (max - min) / 10
            };
            tb.ValueChanged += (s, e) =>
            {
                onChange(tb.Value);
                lbl.Text = $"{title}: {tb.Value}";
            };
            this.Controls.Add(tb);
            return (tb, lbl);
        }

        // ==============================================================
        // 매 프레임 처리
        // ==============================================================
        private void UpdateFrame(object sender, EventArgs e)
        {
            if (capture == null || !capture.IsOpened()) return;
            capture.Read(frame);
            if (frame.Empty()) return;

            try { ProcessAndDisplay(frame); }
            catch (Exception ex) { Debug.WriteLine($"Error: {ex.Message}"); }
        }

        private void ProcessAndDisplay(Mat fullFrame)
        {
            int w = fullFrame.Width, h = fullFrame.Height;
            int rw = (int)(w * RoiWRatio), rh = (int)(h * RoiHRatio);
            int rx1 = (w - rw) / 2, ry1 = (h - rh) / 2;
            using var roi = new Mat(fullFrame, new OpenCvSharp.Rect(rx1, ry1, rw, rh));

            string status = "";

            // ── STAGE 1: 옷감 영역 ──
            using var gray = new Mat();
            Cv2.CvtColor(roi, gray, ColorConversionCodes.BGR2GRAY);
            using var blur = new Mat();
            Cv2.GaussianBlur(gray, blur, new OpenCvSharp.Size(5, 5), 0);
            using var bin = new Mat();
            Cv2.Threshold(blur, bin, FabricBinThresh, 255, ThresholdTypes.Binary);

            using var k5 = Cv2.GetStructuringElement(MorphShapes.Rect, new OpenCvSharp.Size(5, 5));
            Cv2.MorphologyEx(bin, bin, MorphTypes.Open, k5);
            Cv2.MorphologyEx(bin, bin, MorphTypes.Close, k5);

            DisplayMat(picFabricMask, bin);

            Cv2.FindContours(bin, out var contours, out _,
                             RetrievalModes.External, ContourApproximationModes.ApproxSimple);

            if (contours.Length == 0)
            {
                status = "[X] STAGE 1 FAIL: No fabric contour";
                DrawAndShow(roi, null, null, null, status);
                return;
            }

            var bestContour = contours.OrderByDescending(c => Cv2.ContourArea(c)).First();
            double fabricContourArea = Cv2.ContourArea(bestContour);

            if (fabricContourArea < FabricMinArea)
            {
                status = $"[X] STAGE 1 FAIL: Fabric area too small ({fabricContourArea:F0} < {FabricMinArea})";
                DrawAndShow(roi, null, null, null, status);
                return;
            }

            var fabricBox = Cv2.BoundingRect(bestContour);
            using var fabricMask = Mat.Zeros(roi.Height, roi.Width, MatType.CV_8UC1).ToMat();
            Cv2.DrawContours(fabricMask, new[] { bestContour }, -1, new Scalar(255), -1);

            using var fabricRoi = new Mat(roi, fabricBox);
            using var fabricHsv = new Mat();
            Cv2.CvtColor(fabricRoi, fabricHsv, ColorConversionCodes.BGR2HSV);
            using var innerMask = new Mat(fabricMask, fabricBox);

            int fabricArea = Cv2.CountNonZero(innerMask);
            if (fabricArea < 100)
            {
                status = $"[X] STAGE 2 FAIL: Inner area too small ({fabricArea})";
                DrawAndShow(roi, fabricBox, null, null, status);
                return;
            }

            // ── STAGE 2: 침식 ──
            int kernelSize = ErodeKernelSize % 2 == 0 ? ErodeKernelSize + 1 : ErodeKernelSize;
            using var erodeK = Cv2.GetStructuringElement(MorphShapes.Rect,
                new OpenCvSharp.Size(kernelSize, kernelSize));
            using var coreMask = new Mat();
            Cv2.Erode(innerMask, coreMask, erodeK, iterations: ErodeIterations);

            int coreArea = Cv2.CountNonZero(coreMask);
            if (coreArea < 100)
            {
                status = $"[!] STAGE 2: Core too small after erode ({coreArea})\n" +
                         $"   -> Reduce kernel({kernelSize}) or iterations({ErodeIterations})";
                DrawAndShow(roi, fabricBox, null, null, status);
                return;
            }

            // ── HOLE 영역 분석 ──
            Cv2.Split(fabricHsv, out Mat[] fhsvCh);
            using var fabricV = fhsvCh[2].Clone();
            using var fabricS = fhsvCh[1].Clone();
            foreach (var c in fhsvCh) c.Dispose();

            var meanV = Cv2.Mean(fabricV, coreMask);
            double avgV = meanV.Val0;

            using var darkRegion = new Mat();
            Cv2.Threshold(fabricV, darkRegion, Math.Max(0, avgV - 50), 255, ThresholdTypes.BinaryInv);
            Cv2.BitwiseAnd(darkRegion, coreMask, darkRegion);

            using var brightRegion = new Mat();
            Cv2.Threshold(fabricV, brightRegion, Math.Min(255, avgV + 40), 255, ThresholdTypes.Binary);
            using var lowSatMask = new Mat();
            Cv2.Threshold(fabricS, lowSatMask, 50, 255, ThresholdTypes.BinaryInv);
            Cv2.BitwiseAnd(brightRegion, lowSatMask, brightRegion);
            Cv2.BitwiseAnd(brightRegion, coreMask, brightRegion);

            using var holeViz = Mat.Zeros(roi.Height, roi.Width, MatType.CV_8UC3).ToMat();
            using var holeVizFull = new Mat(holeViz, fabricBox);
            holeVizFull.SetTo(new Scalar(0, 0, 255), darkRegion);
            holeVizFull.SetTo(new Scalar(255, 0, 0), brightRegion);
            DisplayMat(picHoleAnalysis, holeViz);

            int darkPixels = Cv2.CountNonZero(darkRegion);
            int brightPixels = Cv2.CountNonZero(brightRegion);

            // ── STAGE 3: HSV 차분 stain ──
            using var blurHsv = new Mat();
            Cv2.GaussianBlur(fabricHsv, blurHsv, new OpenCvSharp.Size(21, 21), 0);

            Cv2.Split(fabricHsv, out Mat[] ch1);
            Cv2.Split(blurHsv, out Mat[] ch2);

            using var diffH = new Mat(); Cv2.Absdiff(ch1[0], ch2[0], diffH);
            using var diffS = new Mat(); Cv2.Absdiff(ch1[1], ch2[1], diffS);
            using var diffV = new Mat(); Cv2.Absdiff(ch1[2], ch2[2], diffV);

            using var hMask = new Mat(); Cv2.Compare(diffH, new Scalar(StainHDiff), hMask, CmpTypes.GT);
            using var sMask = new Mat(); Cv2.Compare(diffS, new Scalar(StainSDiff), sMask, CmpTypes.GT);
            using var vMask = new Mat(); Cv2.Compare(diffV, new Scalar(StainVDiff), vMask, CmpTypes.GT);

            using var colorDiff = new Mat();
            Cv2.BitwiseOr(hMask, sMask, colorDiff);
            Cv2.BitwiseOr(colorDiff, vMask, colorDiff);

            using var brightExclude = new Mat();
            Cv2.Compare(ch1[2], new Scalar(StainBrightExcludeV), brightExclude, CmpTypes.LE);

            using var stainMask = new Mat();
            Cv2.BitwiseAnd(colorDiff, coreMask, stainMask);
            Cv2.BitwiseAnd(stainMask, brightExclude, stainMask);

            Cv2.MorphologyEx(stainMask, stainMask, MorphTypes.Open, k5);
            Cv2.MorphologyEx(stainMask, stainMask, MorphTypes.Close, k5);

            int stainPixels = Cv2.CountNonZero(stainMask);

            using var fullStainMask = Mat.Zeros(roi.Height, roi.Width, MatType.CV_8UC1).ToMat();
            stainMask.CopyTo(fullStainMask[fabricBox]);
            DisplayMat(picStainMask, fullStainMask);

            foreach (var c in ch1) c.Dispose();
            foreach (var c in ch2) c.Dispose();

            // ── STAGE 4: 컨투어 ──
            Cv2.FindContours(stainMask, out var stainCnts, out _,
                             RetrievalModes.External, ContourApproximationModes.ApproxSimple);

            var stainBoxes = new List<OpenCvSharp.Rect>();
            int filteredByArea = 0;
            foreach (var cnt in stainCnts)
            {
                double area = Cv2.ContourArea(cnt);
                if (area < StainMinArea) { filteredByArea++; continue; }
                var r = Cv2.BoundingRect(cnt);
                stainBoxes.Add(new OpenCvSharp.Rect(fabricBox.X + r.X, fabricBox.Y + r.Y, r.Width, r.Height));
            }

            Cv2.FindContours(darkRegion, out var darkCnts, out _,
                             RetrievalModes.External, ContourApproximationModes.ApproxSimple);
            Cv2.FindContours(brightRegion, out var brightCnts, out _,
                             RetrievalModes.External, ContourApproximationModes.ApproxSimple);
            int darkRegionCount = darkCnts.Count(c => Cv2.ContourArea(c) > 30);
            int brightRegionCount = brightCnts.Count(c => Cv2.ContourArea(c) > 30);

            // ── 상태 정리 ──
            status = $"================================================\n" +
                     $"  Fabric: area {fabricContourArea:F0}, after erode {coreArea}\n" +
                     $"  Avg V (fabric): {avgV:F0}\n" +
                     $"\n" +
                     $"  [HOLE] (panel 4)\n" +
                     $"    Dark  : {darkPixels}px ({darkRegionCount} blobs)\n" +
                     $"    Bright: {brightPixels}px ({brightRegionCount} blobs)\n" +
                     $"\n";

            if (stainBoxes.Count > 0)
            {
                status += $"  [OK] STAIN: {stainBoxes.Count} detected (pixels {stainPixels})\n";
            }
            else if (stainPixels > 0)
            {
                status += $"  [!] STAIN: {stainPixels}px caught but all below area threshold\n";
            }
            else
            {
                status += $"  [OK] STAIN: 0 (no false positive)\n";
            }

            status += $"\n" +
                      $"  Thresholds: H/S/V/Area = {StainHDiff}/{StainSDiff}/{StainVDiff}/{StainMinArea}\n" +
                      $"  Erode     : {kernelSize}x{kernelSize}, {ErodeIterations} iter";

            DrawAndShow(roi, fabricBox, stainBoxes, (darkRegionCount, brightRegionCount), status);
        }

        // ==============================================================
        // 시각화
        // ==============================================================
        private void DrawAndShow(Mat roi, OpenCvSharp.Rect? fabricBox,
                                  List<OpenCvSharp.Rect>? stainBoxes,
                                  (int dark, int bright)? holeInfo,
                                  string status)
        {
            using var display = roi.Clone();

            if (fabricBox.HasValue)
            {
                Cv2.Rectangle(display, fabricBox.Value, new Scalar(0, 255, 0), 2);
                Cv2.PutText(display, "FABRIC",
                    new OpenCvSharp.Point(fabricBox.Value.X, fabricBox.Value.Y - 5),
                    HersheyFonts.HersheySimplex, 0.5, new Scalar(0, 255, 0), 1);
            }

            if (stainBoxes != null)
            {
                foreach (var sb in stainBoxes)
                {
                    Cv2.Rectangle(display, sb, new Scalar(0, 0, 255), 2);
                    Cv2.PutText(display, "STAIN",
                        new OpenCvSharp.Point(sb.X, sb.Y - 3),
                        HersheyFonts.HersheySimplex, 0.4, new Scalar(0, 0, 255), 1);
                }
            }

            if (holeInfo.HasValue && (holeInfo.Value.dark > 0 || holeInfo.Value.bright > 0))
            {
                string txt = $"HOLE? dark={holeInfo.Value.dark} bright={holeInfo.Value.bright}";
                Cv2.PutText(display, txt,
                    new OpenCvSharp.Point(10, display.Height - 10),
                    HersheyFonts.HersheySimplex, 0.5, new Scalar(0, 255, 255), 1);
            }

            DisplayMat(picOriginal, display);

            if (lblStatus.InvokeRequired)
                lblStatus.Invoke(new Action(() => lblStatus.Text = status));
            else
                lblStatus.Text = status;
        }

        private void DisplayMat(PictureBox pb, Mat mat)
        {
            if (pb == null || mat == null || mat.Empty()) return;
            var bmp = BitmapConverter.ToBitmap(mat);
            var oldImg = pb.Image;
            pb.Image = bmp;
            oldImg?.Dispose();
        }
    }
}
