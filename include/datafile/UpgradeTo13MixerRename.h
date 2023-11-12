/*
 * UpgradeMixerRename.h - functor for upgrading data files _
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

#ifndef LMMS_UPGRADE_TO_13_MIXER_RENAME_H
#define LMMS_UPGRADE_TO_13_MIXER_RENAME_H

#include "datafile/DataFileUpgrade.h"


namespace lmms
{

// upgrade functor for 
class UpgradeTo13MixerRename : public DataFileUpgrade
{
public:
	UpgradeTo13MixerRename(DataFile& document) : DataFileUpgrade(document) {}

	void upgrade() override;
};


} // namespace lmms

#endif // LMMS_UPGRADE_TO_13_MIXER_RENAME_H

