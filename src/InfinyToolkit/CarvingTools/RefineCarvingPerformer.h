/*****************************************************************************
 *                 - Copyright (C) - 2020 - InfinyTech3D -                   *
 *                                                                           *
 * This file is part of the InfinyToolkit plugin for the SOFA framework      *
 *                                                                           *
 * Commercial License Usage:                                                 *
 * Licensees holding valid commercial license from InfinyTech3D may use this *
 * file in accordance with the commercial license agreement provided with    *
 * the Software or, alternatively, in accordance with the terms contained in *
 * a written agreement between you and InfinyTech3D. For further information *
 * on the licensing terms and conditions, contact: contact@infinytech3d.com  *
 *                                                                           *
 * GNU General Public License Usage:                                         *
 * Alternatively, this file may be used under the terms of the GNU General   *
 * Public License version 3. The licenses are as published by the Free       *
 * Software Foundation and appearing in the file LICENSE.GPL3 included in    *
 * the packaging of this file. Please review the following information to    *
 * ensure the GNU General Public License requirements will be met:           *
 * https://www.gnu.org/licenses/gpl-3.0.html.                                *
 *                                                                           *
 * Authors: see Authors.txt                                                  *
 * Further information: https://infinytech3d.com                             *
 ****************************************************************************/
#pragma once

#include <InfinyToolkit/CarvingTools/BaseCarvingPerformer.h>

#include <MeshRefinement/TetrahedronRefinementAlgorithms.h>

namespace sofa::infinytoolkit
{

class SOFA_INFINYTOOLKIT_API RefineCarvingPerformer : public BaseCarvingPerformer
{
public:
	RefineCarvingPerformer(TetrahedronSetTopologyContainer::SPtr topo, AdvancedCarvingManager* _carvingMgr);

	virtual ~RefineCarvingPerformer();

	bool initPerformer() override;

	void filterContacts();

	bool runPerformer() override;

	void draw(const core::visual::VisualParams* vparams) override;

protected:
	void simpleCarving();

	void surfaceCarving();

	void surfaceCarving2();

protected:
	TetrahedronRefinementAlgorithms* m_tetraAlgo;

	std::set<unsigned int> m_tetra2Filter;

	sofa::type::vector<unsigned int> m_tetra2Filter2;
	std::set<unsigned int> m_triIdsToFilter;
	std::set<unsigned int> m_triIds;

	Vec3 carvingPosition;
};
					
} // namespace sofa::infinytoolkit
	