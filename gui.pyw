import os
import tkinter as tk
import cv2
import threading
import ttkbootstrap as ttk
from ttkbootstrap.tooltip import ToolTip
from ttkbootstrap.constants import *
from ttkbootstrap.style import Bootstyle
from tkinter import filedialog, messagebox
from pathlib import Path
from PIL import Image, ImageTk
from cli import make_thumbnail_sheet

# CollapsingFrame (from ttkbootstrap gallery) with default collapsed
IMG_PATH = Path(__file__).parent / 'video_preview_generator' / 'assets'

class CollapsingFrame(ttk.Frame):
    """A collapsible frame widget that opens and closes with a click."""
    def __init__(self, master, **kwargs):
        super().__init__(master, **kwargs)
        self.columnconfigure(0, weight=1)
        self.cumulative_rows = 0
        self.images = [
            ttk.PhotoImage(file=IMG_PATH / 'icons8_double_up_24px.png'),
            ttk.PhotoImage(file=IMG_PATH / 'icons8_double_right_24px.png')
        ]

    def add(self, child, title="", bootstyle=PRIMARY, **kwargs):
        if child.winfo_class() != 'TFrame':
            return
        style_color = Bootstyle.ttkstyle_widget_color(bootstyle)
        frm = ttk.Frame(self, bootstyle=style_color)
        frm.grid(row=self.cumulative_rows, column=0, sticky=EW)
        header = ttk.Label(frm, text=title, bootstyle=(style_color, INVERSE))
        header.pack(side=LEFT, fill=BOTH, padx=10)
        btn = ttk.Button(frm, image=self.images[0], bootstyle=style_color,
                         command=lambda c=child: self._toggle_open_close(c))
        btn.pack(side=RIGHT)
        child.btn = btn
        child.grid(row=self.cumulative_rows + 1, column=0, sticky=NSEW)
        # default collapsed
        self._toggle_open_close(child)
        self.cumulative_rows += 2

    def _toggle_open_close(self, child):
        if child.winfo_viewable():
            child.grid_remove()
            child.btn.configure(image=self.images[1])
        else:
            child.grid()
            child.btn.configure(image=self.images[0])
    
# Metadata extraction via OpenCV
def get_video_metadata(filepath):
    cap = cv2.VideoCapture(filepath)
    if not cap.isOpened():
        return {"filename": os.path.basename(filepath), "duration": "00:00:00"}
    fps = cap.get(cv2.CAP_PROP_FPS) or 0
    frame_count = cap.get(cv2.CAP_PROP_FRAME_COUNT) or 0
    duration_s = frame_count / fps if fps else 0
    h, rem = divmod(int(duration_s), 3600)
    m, s = divmod(rem, 60)
    cap.release()
    return {"filename": os.path.basename(filepath),
            "duration": f"{h:02d}:{m:02d}:{s:02d}"}

class VideoPreviewGeneratorApp:
    def __init__(self):
        self.app = ttk.Window(title="Video Preview Generator", themename="darkly")
        self.app.geometry("1280x720")
        # Layout: controls, table, preview
        self.app.columnconfigure(0, weight=1)
        self.app.columnconfigure(1, weight=2)
        self.app.columnconfigure(2, weight=3)
        self.app.rowconfigure(0, weight=1)

        # Controls frame
        controls = ttk.Frame(self.app)
        controls.grid(row=0, column=0, sticky="nsew", padx=10, pady=10)
        controls.columnconfigure(0, weight=1)

        ttk.Button(controls, text="Select Video Files", command=self.select_input_files).grid(
            row=0, column=0, sticky="ew", pady=(0,10))
        ttk.Button(controls, text="Remove Selected File", bootstyle="danger",
                   command=self.remove_selected).grid(row=1, column=0, sticky="ew", pady=(0,10))
        ttk.Button(controls, text="Select Output Directory", command=self.select_output_dir).grid(
            row=2, column=0, sticky="ew", pady=(0,10))

        # Collapsible More Settings
        cf = CollapsingFrame(controls)
        cf.grid(row=3, column=0, sticky="ew")
        settings = ttk.Frame(cf)
        settings.columnconfigure((0,1), weight=1)
        cf.add(settings, title="Optional Settings", bootstyle=PRIMARY)

        row_num = 0

        # Output Image Scaling Factor
        ttk.Label(settings, text="Image Scaling Factor: ").grid()
        self.scaling_factor = ttk.Combobox(settings, values=[1,1.5,2], state="readonly")
        self.scaling_factor.set(1.5)
        self.scaling_factor.grid(row=row_num, column=1, sticky="ew", padx=5, pady=5)
        
        row_num += 1

        # Logo and opacity
        ttk.Label(settings, text="Logo:").grid(row=row_num, column=0, sticky="w", padx=5, pady=5)
        ttk.Button(settings, text="Select Watermark Logo", command=self.select_logo).grid(
            row=row_num, column=1, sticky="ew", padx=5, pady=5)
        row_num += 1

        # Text watermark
        ttk.Label(settings, text="Watermark Text:").grid(row=row_num, column=0, sticky="w", padx=5, pady=5)
        self.text_entry = ttk.Entry(settings)
        self.text_entry.grid(row=row_num, column=1, sticky="ew", padx=5, pady=5)
        row_num += 1
        
        # Font
        ttk.Label(settings, text="Font Size:").grid(row=row_num, column=0, sticky="w", padx=5, pady=5)
        self.font_size = ttk.Entry(settings)
        self.font_size.grid(row=row_num, column=1, sticky="ew", padx=5, pady=5)
        row_num += 1
        ttk.Button(settings, text="Select Font (.ttf)", command=self.select_font).grid(
            row=row_num, column=0, columnspan=2, sticky="ew", padx=5, pady=5)
        row_num += 1

        ttk.Label(settings, text="Logo Opacity:").grid(row=row_num, column=0, sticky="w", padx=5, pady=5)
        self.opacity = tk.DoubleVar(value=1.0)
        slider = ttk.Scale(settings, from_=0.0, to=1.0, variable=self.opacity)
        slider.grid(row=row_num, column=1, sticky="ew", padx=5, pady=5)
        slider.bind('<ButtonRelease-1>',
                    lambda e: self.opacity.set(round(self.opacity.get(), 2)))
        

        # Table of files
        cols = ("filename","duration")
        self.tree = ttk.Treeview(self.app, columns=cols, show="headings")
        for c in cols:
            self.tree.heading(c, text=c.capitalize())
        self.tree.grid(row=0, column=1, sticky="nsew", padx=10, pady=10)
        self.tree.bind('<<TreeviewSelect>>', self.show_preview)

        # Preview canvas
        self.canvas = tk.Canvas(self.app, bg="black")
        self.canvas.grid(row=0, column=2, sticky="nsew", padx=10, pady=10)

        self.generate_btn = ttk.Button(controls, text="Generate Previews", bootstyle="success", command=self.generate_previews)
        self.generate_btn.grid(row=5, column=0, sticky="ew", pady=(10,0))

        self.progress = ttk.Progressbar(controls, mode="indeterminate")

        # State
        self.input_files = []
        self.logo_path = None
        self.font_path = None
        self.current_image = None

    def select_input_files(self):
        files = filedialog.askopenfilenames(title="Select Video Files",
                                            filetypes=[("Video Files","*.mp4;*.mov;*.avi;*.mkv;*.webm")])
        if files:
            self.input_files = list(files)
            self.refresh_table()
            self.output_dir = os.path.dirname(files[0])

    def refresh_table(self):
        self.tree.delete(*self.tree.get_children())
        for fp in self.input_files:
            meta = get_video_metadata(fp)
            self.tree.insert('', 'end', values=(meta['filename'], meta['duration']))
        children = self.tree.get_children()
        self.tree.focus_set()
        if children:
            first = children[0]
            self.tree.selection_set(first)   # highlight
            self.tree.focus(first)           # give keyboard focus
            self.tree.see(first)             # scroll into view

    def remove_selected(self):
        sel = self.tree.selection()
        for item in sel:
            idx = self.tree.index(item)
            self.tree.delete(item)
            del self.input_files[idx]
        self.canvas.delete('all')

    def select_output_dir(self):
        directory = filedialog.askdirectory(initialdir=getattr(self, 'output_dir', None),
                                            title="Select Output Directory")
        if directory:
            self.output_dir = directory

    def select_logo(self):
        file = filedialog.askopenfilename(title="Select Watermark Logo",
                                          filetypes=[("Image Files","*.png;*.jpg;*.jpeg")])
        if file:
            self.logo_path = file

    def select_font(self):
        file = filedialog.askopenfilename(title="Select Font File",
                                          filetypes=[("TTF Files","*.ttf")])
        if file:
            self.font_path = file

    def show_preview(self, event=None, fraction=0.5):
        sel = self.tree.selection()
        if not sel:
            return
        idx = self.tree.index(sel[0])
        path = self.input_files[idx]
        cap = cv2.VideoCapture(path)
        if not cap.isOpened():
            return

        # clamp fraction to [0, 1]
        fraction = max(0.0, min(1.0, fraction))

        # compute the target frame index
        total_frames = int(cap.get(cv2.CAP_PROP_FRAME_COUNT))
        target_frame = int(total_frames * fraction)

        # seek and read
        cap.set(cv2.CAP_PROP_POS_FRAMES, target_frame)
        ret, frame = cap.read()
        cap.release()
        if not ret:
            return

        # Convert BGR to RGB
        img = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
        h, w = img.shape[:2]

        # Resize to fit canvas
        cw = self.canvas.winfo_width()
        ch = self.canvas.winfo_height()
        scale = min(cw / w, ch / h)
        nw, nh = int(w * scale), int(h * scale)
        img = Image.fromarray(img).resize((nw, nh))

        self.current_image = ImageTk.PhotoImage(img)
        self.canvas.delete('all')
        self.canvas.create_image(
            (cw - nw) // 2,
            (ch - nh) // 2,
            anchor='nw',
            image=self.current_image
        )

    def generate_previews(self):
        print(">> generate_previews() called")
        self.generate_btn.config(text="Generating Previews...", state="disabled")        # create the thread
        
        # show and start the spinner
        self.progress.grid(row=6, column=0, sticky="ew", pady=(5,0))
        self.progress.start(10)   # 10 ms per “step”
        
        # start worker
        t = threading.Thread(
            target=self._generate_previews_worker,
            daemon=True
        )
        print(f">> starting thread {t}")
        t.start()
        print(">> thread.start() returned")

    def _generate_previews_worker(self):
        print(">> _generate_previews_worker() entered")
        if self.font_size.get() == "":
            self.font_size.insert(END, "25")
        if not self.font_size.get().isdigit():
            messagebox.showinfo('Warning', 'Please enter a valid font size.')
            self.font_size.delete(0, END)
            return
        elif int(self.font_size.get()) > 100:
            messagebox.showinfo('Warning', 'Please enter a font size <= 100.')
            self.font_size.delete(0, END)
            return
        for filepath in self.input_files:
            filename = os.path.basename(filepath)     
            no_ext = os.path.splitext(filename)[0]
            try:
                preview_file = no_ext + "_preview.png"
                print(f"Generating {preview_file}")
                if self.output_dir == None:
                    self.output_dir = os.path.dirname(filepath)

                output_path = os.path.join(self.output_dir, preview_file)
                
                make_thumbnail_sheet(
                    filepath,
                    output_path,
                    padding=5,
                    border_size=1,
                    outer_margin=15,
                    scale_factor=float(self.scaling_factor.get()),
                    font_path=self.font_path, # should default to none if not given
                    logo_path=self.logo_path, # should default to none if not given
                    logo_opacity=float(self.opacity.get()),
                    watermark_text=self.text_entry.get(),
                    watermark_font_size=int(self.font_size.get()),
                    watermark_text_opacity=float(self.opacity.get()),
                    #logo_align="right"
                )
            except Exception as e:
                print(f"something went wrong: {e}")
        print(">> worker done, scheduling callback")
        self.app.after(0, self._on_generate_done)

    def _on_generate_done(self):
        # stop and hide the spinner
        self.progress.stop()
        self.progress.grid_remove()
        # restore button text & state
        self.generate_btn.config(text="Generate Previews", state="normal")
        messagebox.showinfo('Task Finished',
                            'Finished generating your preview images.')

    def run(self):
        self.app.mainloop()

if __name__ == "__main__":
    VideoPreviewGeneratorApp().run()