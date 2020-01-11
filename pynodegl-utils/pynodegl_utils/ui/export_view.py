#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# Copyright 2018 GoPro Inc.
#
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.
#

import os.path as op
from PySide2 import QtCore, QtWidgets
from PySide2.QtUiTools import QUiLoader
from fractions import Fraction

from pynodegl_utils.export import Exporter


class ExportView(QtWidgets.QWidget):

    def __init__(self, get_scene_func, config):
        super(ExportView, self).__init__()

        self._ui = QUiLoader().load(__file__[:-2] + 'ui', self)

        self._get_scene_func = get_scene_func
        self._framerate = config.get('framerate')
        self._aspect_ratio = config.get('aspect_ratio')

        self._ui.ofile_text.setText(config.get('export_filename'))

        self._ui.spinbox_width.setValue(config.get('export_width'))
        self._ui.spinbox_height.setValue(config.get('export_height'))
        self._ui.encopts_text.setText(config.get('export_extra_enc_args'))
        self._ui.export_btn = QtWidgets.QPushButton('Export')

        self._ui.warning_label.hide()

        self._ui.ofile_btn.clicked.connect(self._select_ofile)
        self._ui.export_btn.clicked.connect(self._export)
        self._ui.ofile_text.textChanged.connect(self._check_settings)
        self._ui.ofile_text.textChanged.connect(config.set_export_filename)
        self._ui.spinbox_width.valueChanged.connect(self._check_settings)
        self._ui.spinbox_width.valueChanged.connect(config.set_export_width)
        self._ui.spinbox_height.valueChanged.connect(self._check_settings)
        self._ui.spinbox_height.valueChanged.connect(config.set_export_height)
        self._ui.encopts_text.textChanged.connect(config.set_export_extra_enc_args)

        self._exporter = None

    def enter(self):
        cfg = self._get_scene_func()
        if not cfg:
            return

        self._framerate = cfg['framerate']
        self._aspect_ratio = cfg['aspect_ratio']

        self._check_settings()

    @QtCore.Slot()
    def _check_settings(self):

        warnings = []

        if self._ofile_text.text().endswith('.gif'):
            fr = self._framerate
            gif_recommended_framerate = (Fraction(25, 1), Fraction(50, 1))
            if Fraction(*fr) not in gif_recommended_framerate:
                gif_framerates = ', '.join('%s' % x for x in gif_recommended_framerate)
                warnings.append('It is recommended to use one of these frame rate when exporting to GIF: {}'.format(gif_framerates))

        if warnings:
            self._ui.warning_label.setText('\n'.join('⚠ ' + w for w in warnings))
            self._ui.warning_label.show()
        else:
            self._ui.warning_label.hide()

    @QtCore.Slot(int)
    def _progress(self, value):
        self._pgd.setValue(value)

    @QtCore.Slot()
    def _cancel(self):
        # Exporter.cancel() gracefuly stops the exporter thread and fires a
        # finished signal
        if self._exporter:
            self._exporter.cancel()

    @QtCore.Slot()
    def _fail(self):
        self._finish()
        QtWidgets.QMessageBox.critical(self,
                                       'Error',
                                       "You didn't select any scene to export.",
                                       QtWidgets.QMessageBox.Ok)

    @QtCore.Slot()
    def _finish(self):
        self._pgd.close()
        if self._exporter:
            self._exporter.wait()
            self._exporter = None

    @QtCore.Slot()
    def _export(self):
        if self._exporter:
            return

        ofile = self._ofile_text.text()
        width = self._spinbox_width.value()
        height = self._spinbox_height.value()
        extra_enc_args = self._encopts_text.text().split()

        self._pgd = QtWidgets.QProgressDialog('Exporting to %s' % ofile, 'Stop', 0, 100, self)
        self._pgd.setWindowModality(QtCore.Qt.WindowModal)
        self._pgd.setMinimumDuration(100)

        self._exporter = Exporter(self._get_scene_func, ofile, width, height, extra_enc_args)

        self._pgd.canceled.connect(self._cancel)
        self._exporter.progressed.connect(self._progress)
        self._exporter.failed.connect(self._fail)
        self._exporter.finished.connect(self._finish)

        self._exporter.start()

    @QtCore.Slot()
    def _select_ofile(self):
        filenames = QtWidgets.QFileDialog.getSaveFileName(self, 'Select export file')
        if not filenames[0]:
            return
        self._ofile_text.setText(filenames[0])
