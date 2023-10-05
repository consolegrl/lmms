/*
 * SlicerTUI.h - declaration of class SlicerTUI
 *
 * Copyright (c) 2006-2008 Andreas Brandmaier <andy/at/brandmaier/dot/de>
 *
 * This file is part of LMMS - https://lmms.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program (see COPYING); if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA.
 *
 */

#ifndef SLICERT_UI_H
#define SLICERT_UI_H

#include <QPushButton>

#include "ComboBox.h"
#include "Instrument.h"
#include "InstrumentView.h"
#include "Knob.h"
#include "LcdSpinBox.h"
#include "PixmapButton.h"
#include "WaveForm.h"

namespace lmms {

class SlicerT;

namespace gui {

// style knob, defined in data/themes/default/style.css#L949
class SlicerTKnob : public Knob
{
public:
	SlicerTKnob(QWidget* _parent)
		: Knob(KnobType::Styled, _parent)
	{
		setFixedSize(50, 40);
		setCenterPointX(24.0);
		setCenterPointY(15.0);
	}
};

class SlicerTUI : public InstrumentViewFixedSize
{
	Q_OBJECT

public:
	SlicerTUI(SlicerT* instrument, QWidget* parent);

protected slots:
	void exportMidi();

protected:
	virtual void dragEnterEvent(QDragEnterEvent* _dee);
	virtual void dropEvent(QDropEvent* _de);

	virtual void paintEvent(QPaintEvent* pe);

private:
	SlicerT* m_slicerTParent;

	// lmms UI
	SlicerTKnob m_noteThresholdKnob;
	SlicerTKnob m_fadeOutKnob;
	LcdSpinBox m_bpmBox;
	ComboBox m_snapSetting;

	// buttons
	PixmapButton m_resetButton;
	PixmapButton m_midiExportButton;

	WaveForm m_wf;
};
} // namespace gui
} // namespace lmms
#endif // SLICERT_UI_H
