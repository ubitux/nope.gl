#!/usr/bin/env python

import tkinter as tk
import tkinter.ttk as ttk

import pynodegl as ngl

class _NGLWidget(tk.Canvas):

    def __init__(self, *args, **kwargs):
        tk.Canvas.__init__(self, *args, **kwargs)
        self._viewer = ngl.Viewer()
        self._time = 0
        self.bind("<Configure>", self._configure)
        self.bind("<Expose>", self._draw)
        #self.bind("<Activate>", self._draw)
        #self.bind("<Deactivate>", self._draw)
        #self.bind("<Enter>", self._draw)
        #self.bind("<Leave>", self._draw)
        #self.bind("<Visibility>", self._draw)
        #self.bind("<FocusIn>", self._draw)
        #self.bind("<FocusOut>", self._draw)
        #self.bind("<ResizeRequest>", self._draw)

    @staticmethod
    def get_viewport(width, height, aspect_ratio):
        view_width = width
        view_height = width * aspect_ratio[1] / aspect_ratio[0]
        if view_height > height:
            view_height = height
            view_width = height * aspect_ratio[0] / aspect_ratio[1]
        view_x = (width - view_width) // 2
        view_y = (height - view_height) // 2
        return (view_x, view_y, view_width, view_height)

    @staticmethod
    def _get_scene(media='/home/ux/samples/big_buck_bunny_1080p_h264.mov'):
        #m0 = cfg.medias[0]
        #cfg.duration = m0.duration
        #cfg.aspect_ratio = (m0.width, m0.height)

        q = ngl.Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))
        m = ngl.Media(media)
        t = ngl.Texture2D(data_src=m)
        render = ngl.Render(q)
        render.update_textures(tex0=t)
        return render

    def _configure(self, event):
        print('configure', event)

        #self.wait_visibility()

        self._wid = self.winfo_id()
        self._width = self.winfo_width()
        self._height = self.winfo_height()
        self._aspect_ratio = (1, 1)
        print(self._wid, self._width, self._height)

        self._viewer.configure(window=self._wid,
                         width=self._width,
                         height=self._height,
                         viewport=self.get_viewport(self._width, self._height, self._aspect_ratio),
                         swap_interval=1)
        scene = self._get_scene()
        self._viewer.set_scene(scene)
        self._refresh()

    def _refresh(self):
        self._viewer.draw(self._time)

    def _draw(self, event):
        print('draw', event.type)
        self._refresh()

    def seek(self, value):
        self._time = float(value)
        self._refresh()


if __name__ == '__main__':
    window = tk.Tk()
    window.title('hello')
    window.geometry('320x240')

    style = ttk.Style()
    style.theme_use('clam')

    nglw = _NGLWidget(window)
    nglw.pack(fill=tk.BOTH, expand=tk.YES)

    w = ttk.Scale(window, from_=0, to=100, command=nglw.seek, orient=tk.HORIZONTAL)
    w.pack(fill=tk.X)

    window.mainloop()
