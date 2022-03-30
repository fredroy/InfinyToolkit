/*****************************************************************************
 *            Copyright (C) - InfinyTech3D - All Rights Reserved             *
 *                                                                           *
 * Unauthorized copying of this file, via any medium is strictly prohibited  *
 * Proprietary and confidential.                                             *
 *                                                                           *
 * Written by Erik Pernod <erik.pernod@infinytech3d.com>, October 2019       *
 ****************************************************************************/

#include <InteractionTools/CarvingTools/BaseCarvingPerformer.h>
#include <sofa/core/behavior/BaseMechanicalState.h>
#include <sofa/core/visual/VisualParams.h>

namespace sofa::component::controller
{

BaseCarvingPerformer::BaseCarvingPerformer(TetrahedronSetTopologyContainer::SPtr topo, const SReal& carvingDistance, const SReal& refineDistance)
    : m_topologyCon(topo)
    , m_carvingDistance(carvingDistance)
    , m_refineDistance(refineDistance)
{

}


BaseCarvingPerformer::~BaseCarvingPerformer()
{
    std::cout << "BaseCarvingPerformer::~BaseCarvingPerformer()" << std::endl;
    clearContacts();
}


void BaseCarvingPerformer::clearContacts()
{
    for (unsigned int i = 0; i < m_triangleContacts.size(); i++)
    {
        delete m_triangleContacts[i];
        m_triangleContacts[i] = nullptr;
    }
    m_triangleContacts.clear();

    for (unsigned int i = 0; i < m_pointContacts.size(); i++)
    {
        delete m_pointContacts[i];
        m_pointContacts[i] = nullptr;
    }
    m_pointContacts.clear();
}


void BaseCarvingPerformer::draw(const core::visual::VisualParams* vparams)
{
    if (!m_triangleContacts.empty())
    {
        for each (contactInfo * cInfo in m_triangleContacts)
        {
            std::vector<Vector3> pos;
            sofa::core::behavior::BaseMechanicalState* mstate = m_topologyCon->getContext()->getMechanicalState();
            sofa::core::topology::Topology::Triangle tri = m_topologyCon->getTriangle(cInfo->elemId);

            for (unsigned int j = 0; j < 3; j++) {
                pos.push_back(Vector3(mstate->getPX(tri[j]), mstate->getPY(tri[j]), mstate->getPZ(tri[j])));
            }

            sofa::type::RGBAColor color4(1.0f, 0.0, 0.0f, 1.0);
            if (cInfo->dist < m_carvingDistance)
                color4 = sofa::type::RGBAColor(0.0f, 1.0, 0.0f, 1.0);
            else if (cInfo->dist < m_refineDistance)
                color4 = sofa::type::RGBAColor(0.0f, 0.0, 1.0f, 1.0);

            vparams->drawTool()->drawTriangle(pos[0], pos[1], pos[2], cInfo->normal, color4);
        }
    }

    if (!m_pointContacts.empty())
    {

        for each (contactInfo * cInfo in m_pointContacts)
        {
            std::vector<Vector3> pos;
            sofa::type::RGBAColor color4(1.0f, 0.0, 0.0f, 1.0);
            if (cInfo->dist < m_carvingDistance)
                color4 = sofa::type::RGBAColor(0.0f, 1.0, 0.0f, 1.0);
            else if (cInfo->dist < m_refineDistance)
                color4 = sofa::type::RGBAColor(0.0f, 0.0, 1.0f, 1.0);

            vparams->drawTool()->drawSphere(cInfo->pointB, 0.05f, color4);
            vparams->drawTool()->drawLine(cInfo->pointB, cInfo->pointB + cInfo->normal, sofa::type::RGBAColor(1.0, 0.0, 1.0f, 1.0));
        }

        contactInfo* cInfo = m_pointContacts[0];
        vparams->drawTool()->drawSphere(cInfo->pointA, m_refineDistance, sofa::type::RGBAColor(0.0f, 0.0f, 1.0f, 0.8f));
        vparams->drawTool()->drawSphere(cInfo->pointA, m_carvingDistance, sofa::type::RGBAColor(0.0f, 1.0f, 0.0f, 0.8f));
    }
}

} // namespace sofa::component::controller
