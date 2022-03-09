#!/usr/bin/env python
# -*- coding: utf-8 -*-

import pytest
import os

"""
Test PDF export dialog in board editor

Meant as an addition to the tests in test_schematic_pdf_export.py just to
test the board editor specific functionality works as well.
"""


@pytest.fixture
def dialog(project_editor):
    """
    Fixture opening the PDF export dialog in the board editor
    """
    project_editor.action('boardEditorActionExportPdf').\
        trigger(blocking=False)
    dialog = project_editor.widget('boardEditorGraphicsExportDialog')

    # Do not open exported files since this would be annoying here.
    project_editor.widget(
        'boardEditorGraphicsExportDialogOpenFileCheckbox').\
        set_property('checked', False)

    yield dialog


def test_export_pdf(dialog, project_editor, librepcb, helpers):
    """
    Test if the accept button asks for a filepath and then creates the PDF file
    """
    project_editor.widget(
        'boardEditorGraphicsExportDialogButtonAccept').click()
    path = librepcb.abspath('test export.pdf')
    project_editor.widget(
        'boardEditorGraphicsExportSaveFileDialogFileNameEdit').\
        set_property('text', path)
    project_editor.widget(
        'boardEditorGraphicsExportSaveFileDialogOkButton').click()
    helpers.wait_until_widget_hidden(dialog)  # raises on timeout
    assert os.path.exists(path)
