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

from PySide2 import QtCore, QtGui, QtWidgets
from PySide2.QtUiTools import QUiLoader


class SerialView(QtWidgets.QWidget):

    def __init__(self, get_scene_func):
        super(SerialView, self).__init__()

        self._ui = QUiLoader().load(__file__[:-2] + 'ui', self)

        self._get_scene_func = get_scene_func

        self._ui.text.setFont(QtGui.QFontDatabase.systemFont(QtGui.QFontDatabase.FixedFont))
        self._ui.save_btn.clicked.connect(self._save_to_file)

    @QtCore.Slot()
    def _save_to_file(self):
        data = self._ui.text.toPlainText()
        filenames = QtWidgets.QFileDialog.getSaveFileName(self, 'Select export file')
        if not filenames[0]:
            return
        open(filenames[0], 'w').write(data)

    def enter(self):
        cfg = self._get_scene_func()
        if not cfg:
            return
        self._ui.text.setPlainText(cfg['scene'])
