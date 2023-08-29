/*
 * Copyright 2023 Clément Bœsch <u pkh.me>
 * Copyright 2023 Nope Foundry
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import QtQuick.Templates as T
import Qt.labs.qmlmodels // DelegateChooser is experimental

ApplicationWindow {
    visible: true
    minimumWidth: 640
    minimumHeight: 480

    signal exportVideo(string filename, int res, int profile)
    signal cancelExport()

    FileDialog {
        id: scriptDialog
        // currentFolder: StandardPaths.standardLocations(StandardPaths.PicturesLocation)[0]
        nameFilters: ["Python scripts (*.py)"]
        onAccepted: {
            script.text = selectedFile
            script.editingFinished()
        }
    }


    /*
     * Export
     */

    FileDialog {
        id: exportDialog
        // currentFolder: StandardPaths.standardLocations(StandardPaths.PicturesLocation)[0]
        nameFilters: ["MP4 (*.mp4)", "Animated GIF (*.gif)"]
        fileMode: FileDialog.SaveFile
        onAccepted: {
            exportFile.text = selectedFile
            exportFile.editingFinished()
        }
    }

    Popup {
        id: exportPopup

        modal: true
        closePolicy: Popup.NoAutoClose

        anchors.centerIn: parent
        StackLayout {
            id: exportStack

            // 0: Exporting
            ColumnLayout {
                Text { text: "Exporting " + exportFile.text }
                ProgressBar {
                    objectName: "exportBar"
                    id: exportBar
                    Layout.fillWidth: true
                }
                Button {
                    text: "Cancel"
                    Layout.alignment: Qt.AlignHCenter
                    onClicked: {
                        cancelExport()
                        exportPopup.visible = false
                    }
                }
            }

            // 1: Export succeed
            ColumnLayout {
                Item { Layout.fillWidth: true; Layout.fillHeight: true } // spacer
                Text { text: exportFile.text + " export done." }
                Item { Layout.fillWidth: true; Layout.fillHeight: true } // spacer
                Button {
                    text: "OK"
                    Layout.alignment: Qt.AlignHCenter
                    onClicked: exportPopup.visible = false
                }
            }

            // 2: Export failed
            ColumnLayout {
                Text { text: exportFile.text + " export failed."; color: "red" }
                Text { id: exportErrorText }
                Item { Layout.fillWidth: true; Layout.fillHeight: true } // spacer
                Button {
                    text: "OK"
                    Layout.alignment: Qt.AlignHCenter
                    onClicked: exportPopup.visible = false
                }
            }
        }
    }

    function start_export(filename, res, profile) {
        exportBar.value = 0
        exportStack.currentIndex = 0
        exportPopup.visible = true
        exportVideo(filename, res, profile)
    }

    function done_export() {
        exportStack.currentIndex = 1
    }

    function error_export(msg) {
        exportErrorText.text = msg
        exportStack.currentIndex = 2
    }

    function disable_export(msg) {
        exportQueryError.text = msg
        exportQueryStack.currentIndex = 1
    }

    /*
     * Main view
     */

    SplitView {
        anchors.fill: parent
        // anchors.margins: 5

        // ScrollView {
        //     ScrollBar.horizontal.interactive: true
        //     ScrollBar.vertical.interactive: true
            ColumnLayout {
                SplitView.fillWidth: false

                Frame {
                    Layout.fillWidth: true
                    Layout.margins: 5

                    contentItem: GridLayout {
                        columns: 2

                        Text { text: "Script:" }
                        RowLayout {
                            TextField {
                                Layout.fillWidth: true
                                id: script; objectName: "script" }
                            Button {
                                text: "Open"
                                background.implicitWidth: 0
                                background.implicitHeight: 0
                                onClicked: scriptDialog.open()
                            }
                        }

                        Text { text: "Scene:" }
                        ComboBox {
                            Layout.fillWidth: true
                            objectName: "sceneList"
                        }
                    }
                }

                GroupBox {
                    title: "Rendering settings"
                    Layout.fillWidth: true
                    Layout.margins: 5

                    contentItem: GridLayout {
                        columns: 2

                        Text { text: "MSAA:" }
                        ComboBox {
                            objectName: "samplesList"
                            Layout.fillWidth: true
                        }

                        Text { text: "Framerate:" }
                        ComboBox {
                            objectName: "framerateList"
                            Layout.fillWidth: true
                        }
                    }
                }

                GroupBox {
                    title: "Build scene options"
                    Layout.fillWidth: true
                    Layout.margins: 5
                }

                GroupBox {
                    title: "Live scene controls"
                    Layout.fillWidth: true
                    Layout.margins: 5
                    // visible: controlList.model.length > 0

                    contentItem: ListView {
                        id: controlList
                        objectName: "controlList"

                        implicitWidth: contentWidth
                        implicitHeight: contentHeight

                        delegate: DelegateChooser {
                            role: "type"
                            DelegateChoice {
                                roleValue: "range"
                                RowLayout {
                                    Text { text: model.label }
                                    Slider {
                                        from: model.min
                                        to: model.max
                                        value: model.val
                                        onMoved: model.val = value
                                    }
                                    Text { text: model.val }
                                }
                            }
                            DelegateChoice {
                                roleValue: "color"
                                Button {
                                    ColorDialog {
                                        id: color_dialog
                                        selectedColor: model.val
                                        onSelectedColorChanged: model.val = selectedColor
                                        options:  ColorDialog.NoButtons
                                    }
                                    text: model.label
                                    palette.button: color_dialog.selectedColor
                                    onClicked: color_dialog.open()
                                }
                            }
                            DelegateChoice {
                                roleValue: "bool"
                                Switch {
                                    checked: model.val
                                    onToggled: model.val = checked
                                    text: model.label
                                }
                            }
                            DelegateChoice {
                                roleValue: "text"
                                RowLayout {
                                    Text { text: model.label }
                                    TextField {
                                        text: model.val
                                        onEditingFinished: model.val = text
                                    }
                                }
                            }
                        }
                    }
                }

                /* Spacer */
                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }

                GroupBox {
                    title: "Export as video"
                    Layout.fillWidth: true
                    Layout.margins: 5

                    contentItem: StackLayout {
                        id: exportQueryStack

                        // 0: Export form
                        ColumnLayout {
                            GridLayout {
                                columns: 2

                                Text { text: "File:" }
                                RowLayout {
                                    TextField { id: exportFile; text: "output.mp4"; Layout.fillWidth: true }
                                    Button {
                                        text: "Open"
                                        background.implicitWidth: 0
                                        background.implicitHeight: 0
                                        onClicked: exportDialog.open()
                                    }
                                }

                                Text { text: "Res:" }
                                ComboBox { id: resList; objectName: "resList"; Layout.fillWidth: true }

                                Text { text: "Profile:" }
                                ComboBox { id: profileList; objectName: "profileList"; Layout.fillWidth: true}
                            }
                            Button {
                                text: "Export"
                                Layout.alignment: Qt.AlignHCenter
                                onClicked: start_export(exportFile.text, resList.currentIndex, profileList.currentIndex)
                            }
                        }

                        // 1: Export not available
                        Text { id: exportQueryError }
                    }
                }
            }
        // }

        ColumnLayout {
            Player {
                Layout.margins: 5
                SplitView.fillWidth: true
                objectName: "player"
            }
        }
    }
}
