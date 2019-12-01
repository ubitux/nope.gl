#!/usr/bin/env python

import tkinter as tk
import tkinter.ttk as ttk


class _Segment:

    _EDGE_W = 10
    _CENTER_W = 20

    W = _CENTER_W + _EDGE_W * 2

    def __init__(self, canvas, x, color):
        self._canvas = canvas

        y1 = canvas.y1
        y2 = canvas.y2

        edge_l_x1 = x
        edge_l_x2 = x + self._EDGE_W

        center_x1 = edge_l_x2
        center_x2 = edge_l_x2 + self._CENTER_W

        edge_r_x1 = center_x2
        edge_r_x2 = center_x2 + self._EDGE_W

        self._center = canvas.create_rectangle(center_x1, y1, center_x2, y2, fill=color)
        self._edge_l = canvas.create_rectangle(edge_l_x1, y1, edge_l_x2, y2, fill=color, stipple='gray25')
        self._edge_r = canvas.create_rectangle(edge_r_x1, y1, edge_r_x2, y2, fill=color, stipple='gray25')

        self.x1 = edge_l_x1
        self.x2 = edge_r_x2

        canvas.tag_bind(self._edge_l, '<B1-Motion>', self._resize_left)
        canvas.tag_bind(self._edge_r, '<B1-Motion>', self._resize_right)
        canvas.tag_bind(self._center, '<B1-Motion>', self._move)

        print(f'Created segment ({self.x1}->{self.x2})')

    def remove(self):
        self._canvas.delete(self._center)
        self._canvas.delete(self._edge_l)
        self._canvas.delete(self._edge_r)

    def update_y(self, y1, y2):
        for shape in {self._edge_l, self._edge_r, self._center}:
            x1, _, x2, _ = self._canvas.coords(shape)
            self._canvas.coords(shape, x1, y1, x2, y2)

    def _move(self, event):
        half_w = (self.x2 - self.x1) / 2

        min_x = self._canvas.get_min_x(self.x1, segment_exclude=self) + half_w
        max_x = self._canvas.get_max_x(self.x2, segment_exclude=self) - half_w

        print(f'x:{event.x} min_x:{min_x} max_x:{max_x}')
        x = min(max(event.x, min_x), max_x)

        center_x1 = x - half_w + self._EDGE_W
        center_x2 = x + half_w - self._EDGE_W

        edge_l_x2 = center_x1
        edge_l_x1 = center_x1 - self._EDGE_W

        edge_r_x1 = center_x2
        edge_r_x2 = center_x2 + self._EDGE_W

        y1, y2 = self._canvas.y1, self._canvas.y2
        self._canvas.coords(self._edge_l, edge_l_x1, y1, edge_l_x2, y2)
        self._canvas.coords(self._edge_r, edge_r_x1, y1, edge_r_x2, y2)
        self._canvas.coords(self._center, center_x1, y1, center_x2, y2)

        self.x1 = edge_l_x1
        self.x2 = edge_r_x2

    def _resize_right(self, event):
        min_x = self.x1 + self.W
        max_x = self._canvas.get_max_x(self.x2, segment_exclude=self)

        print(f'x:{event.x} min_x:{min_x} max_x:{max_x}')
        x = min(max(event.x, min_x), max_x)

        center_x1, y1, _, y2 = self._canvas.coords(self._center)
        edge_r_x2 = x
        edge_r_x1 = edge_r_x2 - self._EDGE_W
        center_x2 = edge_r_x1
        self._canvas.coords(self._center, center_x1, y1, center_x2, y2)
        self._canvas.coords(self._edge_r, edge_r_x1, y1, edge_r_x2, y2)
        self.x2 = x

    def _resize_left(self, event):
        min_x = self._canvas.get_min_x(self.x1, segment_exclude=self)
        max_x = self.x2 - self.W

        print(f'x:{event.x} min_x:{min_x} max_x:{max_x}')
        x = min(max(event.x, min_x), max_x)

        _, y1, center_x2, y2 = self._canvas.coords(self._center)
        edge_l_x1 = x
        edge_l_x2 = edge_l_x1 + self._EDGE_W
        center_x1 = edge_l_x2
        self._canvas.coords(self._edge_l, edge_l_x1, y1, edge_l_x2, y2)
        self._canvas.coords(self._center, center_x1, y1, center_x2, y2)
        self.x1 = x


class _Z(tk.Canvas):

    _WPAD = 20
    _H = 50

    def __init__(self, *args, **kwargs):
        tk.Canvas.__init__(self, *args, **kwargs, bg='#222222')
        self._rect = None
        self._cur_rect = None

        self._segments = []

        self.bind('<Configure>', self._configure)
        self.bind('<Button-1>', self._create_segment)
        self.bind('<Double-Button-1>', self._kill_segment)

    def _configure(self, event):
        w = event.width
        h = event.height

        self.x1 = self._WPAD
        self.x2 = w - self._WPAD
        self.y1 = (event.height - self._H) / 2
        self.y2 = self.y1 + self._H

        x1, y1, x2, y2 = self.x1, self.y1, self.x2, self.y2
        if self._rect is None:
            self._rect = self.create_rectangle(x1, y1, x2, y2, fill='#333355')
        else:
            self.coords(self._rect, x1, y1, x2, y2)
        for segment in self._segments:
            segment.update_y(y1, y2)

    def set_color(self, color):
        self._color = color

    def _get_seg_block(self, x, dist_func, segment_exclude):
        min_dist = None
        closest = None
        for segment in self._segments:
            if segment == segment_exclude:
                continue
            if segment.x1 <= x <= segment.x2:
                return segment
            dist = dist_func(x, segment)
            if dist > 0 and (min_dist is None or dist < min_dist):
                closest = segment
                min_dist = dist
        return closest

    def get_min_x(self, x, segment_exclude=None):
        dist_func = lambda x, seg: x - seg.x2
        seg = self._get_seg_block(x, dist_func, segment_exclude)
        return seg.x2 if seg else self.x1

    def get_max_x(self, x, segment_exclude=None):
        dist_func = lambda x, seg: seg.x1 - x
        seg = self._get_seg_block(x, dist_func, segment_exclude)
        return seg.x1 if seg else self.x2

    def get_segment_from_x(self, x):
        for segment in self._segments:
            if segment.x1 <= x <= segment.x2:
                return segment
        return None

    def _create_segment(self, event):
        x = event.x

        segment = self.get_segment_from_x(x)
        if segment:
            print('clicked an existing segment, ignoring')
            return

        min_x = self.get_min_x(x)
        max_x = self.get_max_x(x)
        print(f'min_x={min_x} max_x={max_x}')

        avail = max_x - min_x
        if avail < _Segment.W:
            print(f'no space for segment (avail:{avail} need:{_Segment.W})')
            return

        x_start = x - _Segment.W / 2
        if x_start < min_x:
            x_start = min_x
        elif x_start + _Segment.W > max_x:
            x_start = max_x - _Segment.W

        print(f'create segment at x={x_start}')
        segment = _Segment(self, x_start, self._color)
        self._segments.append(segment)

    def _kill_segment(self, event):
        x = event.x
        segment = self.get_segment_from_x(x)
        if segment:
            self._segments.remove(segment)
            segment.remove()


class _Buttons:

    _OK, _KO = range(2)

    def __init__(self, master, z):
        self._z = z
        btn_frame = ttk.Frame(master)
        self._ok_btn = tk.Button(btn_frame, text='☺', fg='white', command=self._set_ok, bg='green')
        self._ko_btn = tk.Button(btn_frame, text='☹', fg='white', command=self._set_ko, bg='red')
        self._ok_btn.pack(side=tk.LEFT)
        self._ko_btn.pack(side=tk.LEFT)
        btn_frame.pack()
        self._set_ok()

    def _set_ko(self):
        self._ok_btn.configure(relief='groove')
        self._ko_btn.configure(relief='sunken')
        self._z.set_color('red')

    def _set_ok(self):
        self._ok_btn.configure(relief='sunken')
        self._ko_btn.configure(relief='groove')
        self._z.set_color('green')


def  _main():
    w = tk.Tk()

    z = _Z(w)
    buttons = _Buttons(w, z)

    z.pack(fill=tk.BOTH, expand=True)

    w.mainloop()

if __name__ == '__main__':
    _main()
