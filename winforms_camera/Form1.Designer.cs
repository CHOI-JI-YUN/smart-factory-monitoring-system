namespace Work1
{
    partial class Form1
    {
        /// <summary>
        ///  Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        ///  Clean up any resources being used.
        /// </summary>
        /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        #region Windows Form Designer generated code

        /// <summary>
        ///  Required method for Designer support - do not modify
        ///  the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(Form1));
            picCamera = new PictureBox();
            panel1 = new Panel();
            dgv_Log = new DataGridView();
            Time = new DataGridViewTextBoxColumn();
            Type = new DataGridViewTextBoxColumn();
            Count = new DataGridViewTextBoxColumn();
            pictureBox2 = new PictureBox();
            label2 = new Label();
            lblStatusText = new Label();
            label1 = new Label();
            label3 = new Label();
            panel2 = new Panel();
            label4 = new Label();
            pictureBox1 = new PictureBox();
            pnlStatusDot = new Panel();
            lblConnStatus = new Label();
            ((System.ComponentModel.ISupportInitialize)picCamera).BeginInit();
            panel1.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)dgv_Log).BeginInit();
            ((System.ComponentModel.ISupportInitialize)pictureBox2).BeginInit();
            panel2.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)pictureBox1).BeginInit();
            SuspendLayout();
            // 
            // picCamera
            // 
            picCamera.Location = new Point(12, 84);
            picCamera.Name = "picCamera";
            picCamera.Size = new Size(640, 436);
            picCamera.SizeMode = PictureBoxSizeMode.StretchImage;
            picCamera.TabIndex = 3;
            picCamera.TabStop = false;
            // 
            // panel1
            // 
            panel1.BackColor = Color.Transparent;
            panel1.Controls.Add(dgv_Log);
            panel1.Controls.Add(pictureBox2);
            panel1.Controls.Add(label2);
            panel1.Location = new Point(658, 40);
            panel1.Name = "panel1";
            panel1.Size = new Size(321, 480);
            panel1.TabIndex = 4;
            // 
            // dgv_Log
            // 
            dgv_Log.AllowUserToAddRows = false;
            dgv_Log.AllowUserToDeleteRows = false;
            dgv_Log.AllowUserToResizeRows = false;
            dgv_Log.BackgroundColor = Color.FromArgb(24, 25, 31);
            dgv_Log.BorderStyle = BorderStyle.None;
            dgv_Log.ColumnHeadersHeightSizeMode = DataGridViewColumnHeadersHeightSizeMode.AutoSize;
            dgv_Log.Columns.AddRange(new DataGridViewColumn[] { Time, Type, Count });
            dgv_Log.Location = new Point(0, 45);
            dgv_Log.Name = "dgv_Log";
            dgv_Log.ReadOnly = true;
            dgv_Log.RowHeadersVisible = false;
            dgv_Log.SelectionMode = DataGridViewSelectionMode.FullRowSelect;
            dgv_Log.Size = new Size(321, 436);
            dgv_Log.TabIndex = 4;
            // 
            // Time
            // 
            Time.HeaderText = "시간";
            Time.Name = "Time";
            Time.ReadOnly = true;
            Time.Width = 120;
            // 
            // Type
            // 
            Type.HeaderText = "종류";
            Type.Name = "Type";
            Type.ReadOnly = true;
            // 
            // Count
            // 
            Count.HeaderText = "갯수";
            Count.Name = "Count";
            Count.ReadOnly = true;
            // 
            // pictureBox2
            // 
            pictureBox2.Image = (Image)resources.GetObject("pictureBox2.Image");
            pictureBox2.Location = new Point(3, 6);
            pictureBox2.Name = "pictureBox2";
            pictureBox2.Size = new Size(35, 35);
            pictureBox2.SizeMode = PictureBoxSizeMode.StretchImage;
            pictureBox2.TabIndex = 3;
            pictureBox2.TabStop = false;
            // 
            // label2
            // 
            label2.AutoSize = true;
            label2.Font = new Font("맑은 고딕", 12F, FontStyle.Regular, GraphicsUnit.Point, 129);
            label2.ForeColor = Color.White;
            label2.Location = new Point(43, 12);
            label2.Name = "label2";
            label2.Size = new Size(80, 21);
            label2.TabIndex = 2;
            label2.Text = "검출 로그";
            // 
            // lblStatusText
            // 
            lblStatusText.AutoSize = true;
            lblStatusText.Font = new Font("맑은 고딕", 12F, FontStyle.Bold, GraphicsUnit.Point, 129);
            lblStatusText.ForeColor = Color.White;
            lblStatusText.Location = new Point(560, 11);
            lblStatusText.Name = "lblStatusText";
            lblStatusText.Size = new Size(17, 21);
            lblStatusText.TabIndex = 3;
            lblStatusText.Text = "-";
            // 
            // label1
            // 
            label1.AutoSize = true;
            label1.BackColor = Color.Transparent;
            label1.Font = new Font("맑은 고딕", 15.75F, FontStyle.Bold, GraphicsUnit.Point, 129);
            label1.ForeColor = Color.White;
            label1.Location = new Point(3, 2);
            label1.Name = "label1";
            label1.Size = new Size(279, 30);
            label1.TabIndex = 5;
            label1.Text = "FABRIC DEFECT DETECTOR";
            // 
            // label3
            // 
            label3.AutoSize = true;
            label3.Font = new Font("맑은 고딕", 12F, FontStyle.Regular, GraphicsUnit.Point, 129);
            label3.ForeColor = Color.White;
            label3.Location = new Point(45, 11);
            label3.Name = "label3";
            label3.Size = new Size(154, 21);
            label3.TabIndex = 2;
            label3.Text = "모니터링 (컨베이어)";
            // 
            // panel2
            // 
            panel2.Controls.Add(label4);
            panel2.Controls.Add(lblStatusText);
            panel2.Controls.Add(pictureBox1);
            panel2.Controls.Add(label3);
            panel2.Location = new Point(3, 40);
            panel2.Name = "panel2";
            panel2.Size = new Size(652, 480);
            panel2.TabIndex = 6;
            // 
            // label4
            // 
            label4.AutoSize = true;
            label4.Font = new Font("맑은 고딕", 12F, FontStyle.Regular, GraphicsUnit.Point, 129);
            label4.ForeColor = Color.White;
            label4.Location = new Point(464, 11);
            label4.Name = "label4";
            label4.Size = new Size(90, 21);
            label4.TabIndex = 2;
            label4.Text = "검출 상태 :";
            // 
            // pictureBox1
            // 
            pictureBox1.Image = (Image)resources.GetObject("pictureBox1.Image");
            pictureBox1.Location = new Point(9, 6);
            pictureBox1.Name = "pictureBox1";
            pictureBox1.Size = new Size(30, 32);
            pictureBox1.SizeMode = PictureBoxSizeMode.StretchImage;
            pictureBox1.TabIndex = 3;
            pictureBox1.TabStop = false;
            // 
            // pnlStatusDot
            // 
            pnlStatusDot.Location = new Point(841, 13);
            pnlStatusDot.Name = "pnlStatusDot";
            pnlStatusDot.Size = new Size(15, 15);
            pnlStatusDot.TabIndex = 7;
            // 
            // lblConnStatus
            // 
            lblConnStatus.AutoSize = true;
            lblConnStatus.Font = new Font("맑은 고딕", 9.75F, FontStyle.Bold, GraphicsUnit.Point, 129);
            lblConnStatus.ForeColor = Color.White;
            lblConnStatus.Location = new Point(862, 12);
            lblConnStatus.Name = "lblConnStatus";
            lblConnStatus.Size = new Size(13, 17);
            lblConnStatus.TabIndex = 8;
            lblConnStatus.Text = "-";
            // 
            // Form1
            // 
            AutoScaleDimensions = new SizeF(7F, 15F);
            AutoScaleMode = AutoScaleMode.Font;
            BackColor = Color.FromArgb(45, 45, 48);
            ClientSize = new Size(987, 526);
            Controls.Add(lblConnStatus);
            Controls.Add(pnlStatusDot);
            Controls.Add(label1);
            Controls.Add(panel1);
            Controls.Add(picCamera);
            Controls.Add(panel2);
            Name = "Form1";
            Text = "FABRIC DEFECT DETECTOR";
            Load += Form1_Load;
            ((System.ComponentModel.ISupportInitialize)picCamera).EndInit();
            panel1.ResumeLayout(false);
            panel1.PerformLayout();
            ((System.ComponentModel.ISupportInitialize)dgv_Log).EndInit();
            ((System.ComponentModel.ISupportInitialize)pictureBox2).EndInit();
            panel2.ResumeLayout(false);
            panel2.PerformLayout();
            ((System.ComponentModel.ISupportInitialize)pictureBox1).EndInit();
            ResumeLayout(false);
            PerformLayout();
        }

        #endregion
        private PictureBox picCamera;
        private Panel panel1;
        private Label label1;
        private Label label2;
        private Label lblStatusText;
        private Label label3;
        private Panel panel2;
        private PictureBox pictureBox1;
        private Label label4;
        private Panel pnlStatusDot;
        private Label lblConnStatus;
        private DataGridView dgv_Log;
        private PictureBox pictureBox2;
        private DataGridViewTextBoxColumn Time;
        private DataGridViewTextBoxColumn Type;
        private DataGridViewTextBoxColumn Count;
    }
}
