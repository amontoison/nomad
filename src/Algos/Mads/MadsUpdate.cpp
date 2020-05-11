/*---------------------------------------------------------------------------------*/
/*  NOMAD - Nonlinear Optimization by Mesh Adaptive Direct Search -                */
/*                                                                                 */
/*  NOMAD - Version 4.0.0 has been created by                                      */
/*                 Viviane Rochon Montplaisir  - Polytechnique Montreal            */
/*                 Christophe Tribes           - Polytechnique Montreal            */
/*                                                                                 */
/*  The copyright of NOMAD - version 4.0.0 is owned by                             */
/*                 Charles Audet               - Polytechnique Montreal            */
/*                 Sebastien Le Digabel        - Polytechnique Montreal            */
/*                 Viviane Rochon Montplaisir  - Polytechnique Montreal            */
/*                 Christophe Tribes           - Polytechnique Montreal            */
/*                                                                                 */
/*  NOMAD v4 has been funded by Rio Tinto, Hydro-Québec, NSERC (Natural            */
/*  Sciences and Engineering Research Council of Canada), InnovÉÉ (Innovation      */
/*  en Énergie Électrique) and IVADO (The Institute for Data Valorization)         */
/*                                                                                 */
/*  NOMAD v3 was created and developed by Charles Audet, Sebastien Le Digabel,     */
/*  Christophe Tribes and Viviane Rochon Montplaisir and was funded by AFOSR       */
/*  and Exxon Mobil.                                                               */
/*                                                                                 */
/*  NOMAD v1 and v2 were created and developed by Mark Abramson, Charles Audet,    */
/*  Gilles Couture, and John E. Dennis Jr., and were funded by AFOSR and           */
/*  Exxon Mobil.                                                                   */
/*                                                                                 */
/*  Contact information:                                                           */
/*    Polytechnique Montreal - GERAD                                               */
/*    C.P. 6079, Succ. Centre-ville, Montreal (Quebec) H3C 3A7 Canada              */
/*    e-mail: nomad@gerad.ca                                                       */
/*    phone : 1-514-340-6053 #6928                                                 */
/*    fax   : 1-514-340-5665                                                       */
/*                                                                                 */
/*  This program is free software: you can redistribute it and/or modify it        */
/*  under the terms of the GNU Lesser General Public License as published by       */
/*  the Free Software Foundation, either version 3 of the License, or (at your     */
/*  option) any later version.                                                     */
/*                                                                                 */
/*  This program is distributed in the hope that it will be useful, but WITHOUT    */
/*  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or          */
/*  FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License    */
/*  for more details.                                                              */
/*                                                                                 */
/*  You should have received a copy of the GNU Lesser General Public License       */
/*  along with this program. If not, see <http://www.gnu.org/licenses/>.           */
/*                                                                                 */
/*  You can find information on the NOMAD software at www.gerad.ca/nomad           */
/*---------------------------------------------------------------------------------*/

#include "../../Algos/CacheInterface.hpp"
#include "../../Algos/Mads/MadsUpdate.hpp"
#include "../../Output/OutputInfo.hpp"

void NOMAD::MadsUpdate::init()
{
    _name = getAlgoName() + "Update";
    verifyParentNotNull();

    auto megaIter = getParentOfType<NOMAD::MadsMegaIteration*>();
    if (nullptr == megaIter)
    {
        throw NOMAD::Exception(__FILE__,__LINE__,"An instance of class MadsUpdate must have a MegaIteration among its ancestors");
    }

}


bool NOMAD::MadsUpdate::runImp()
{
    // megaIter barrier is already in subproblem.
    // So no need to convert refBestFeas and refBestInf
    // from full dimension to subproblem.
    auto megaIter = getParentOfType<NOMAD::MadsMegaIteration*>();
    auto barrier = megaIter->getBarrier();
    auto mesh = megaIter->getMesh();
    std::string s;  // for output

    OUTPUT_DEBUG_START
    s = "Running " + getName() + ". Barrier: ";
    AddOutputDebug(s);
    s = barrier->display(4);
    AddOutputDebug(s);
    OUTPUT_DEBUG_END

    // Barrier is already updated from previous steps.
    // Get ref best feasible and infeasible, and then update 
    // reference values.
    auto refBestFeas = barrier->getRefBestFeas();
    auto refBestInf  = barrier->getRefBestInf();

    barrier->updateRefBests();

    NOMAD::EvalPointPtr newBestFeas = barrier->getFirstXFeas();
    NOMAD::EvalPointPtr newBestInf  = barrier->getFirstXInf();

    if (nullptr != refBestFeas || nullptr != refBestInf)
    {
        // Compute success
        // Get which of newBestFeas and newBestInf is improving
        // the solution. Check newBestFeas first.
        NOMAD::ComputeSuccessType computeSuccess;
        std::shared_ptr<NOMAD::EvalPoint> newBest;
        NOMAD::SuccessType success = computeSuccess(newBestFeas, refBestFeas);
        if (success >= NOMAD::SuccessType::PARTIAL_SUCCESS)
        {
            // newBestFeas is the improving point.
            newBest = newBestFeas;
            OUTPUT_DEBUG_START
            // Output Warning: When using '\n', the computed indentation for the
            // Step will be ignored. Leaving it like this for now. Using an
            // OutputInfo with AddMsg() would resolve the output layout.
            s = "Update: improving feasible point";
            if (refBestFeas)
            {
                s += " from\n    " + refBestFeas->display() + "\n";
            }
            s += " to " + newBestFeas->display();
            AddOutputDebug(s);
            OUTPUT_DEBUG_END
        }
        else
        {
            // Check newBestInf
            NOMAD::SuccessType success2 = computeSuccess(newBestInf, refBestInf);
            if (success2 > success)
            {
                success = success2;
            }
            if (success >= NOMAD::SuccessType::PARTIAL_SUCCESS)
            {
                // newBestInf is the improving point.
                newBest = newBestInf;
                OUTPUT_DEBUG_START
                std::string s = "Update: improving infeasible point";
                if (refBestInf)
                {
                    s+= " from\n    " + refBestInf->display() + "\n";
                }
                s += " to " + newBestInf->display();
                AddOutputDebug(s);
                OUTPUT_DEBUG_END
            }
        }
        if (success == NOMAD::SuccessType::UNSUCCESSFUL)
        {
            OUTPUT_DEBUG_START
            std::string s = "Update: no success found";
            AddOutputDebug(s);
            OUTPUT_DEBUG_END
        }

        // NOTE enlarge or refine might have to be done multiple times
        // in a row, if we are working on multiple meshes at the same
        // time.
        // If we found a better refBest but only for a mesh that is
        // already refined a few times, we must refine mainMesh accordingly
        // before enlarging it.
        // If we found a better refBest for a mesh that is enlarged
        // a few times, we must enlarge mainMesh this many times.
        // If we did not find a better refBest, we refine the mesh
        // once, we might fall on points that are already evaluated - which
        // is fine, or on new points - which will be evaluated.
        //
        // For now, leave it as is, since multiple mesh sizes are not
        // currently evaluated.
        //
        // TODO Analyze, write tests, and implement.


        // Debug verification
        // Compare computed success with value from MegaIteration.
        // This is the value from the previous MegaIteration. If it
        // was not evaluated, ignore the test.
        // It is possible that the MegaIteration found a partial success,
        // and then the EvaluatorControl found a full success before 
        // Update is run.
        // For this reason, only test the boolean value success vs. failure.
        const bool megaIterSuccessful = (megaIter->getSuccessType() >= NOMAD::SuccessType::PARTIAL_SUCCESS);
        const bool successful = (success >= NOMAD::SuccessType::PARTIAL_SUCCESS);
        if (   (NOMAD::SuccessType::NOT_EVALUATED != megaIter->getSuccessType())
            && (   (successful != megaIterSuccessful)
                || (NOMAD::SuccessType::NOT_EVALUATED == success)) )
        {
            std::string s = "Warning: MegaIteration success type: ";
            s += NOMAD::enumStr(megaIter->getSuccessType());
            s += ". Is different than computed success type: " + NOMAD::enumStr(success);
            if (refBestFeas)
            {
                s += "\nRef best feasible:   " + refBestFeas->displayAll();
            }
            if (newBestFeas)
            {
                s += "\nNew best feasible:   " + newBestFeas->displayAll();
            }
            if (refBestInf)
            {
                s += "\nRef best infeasible: " + refBestInf->displayAll();
            }
            if (newBestInf)
            {
                s += "\nNew best infeasible: " + newBestInf->displayAll();
            }
            AddOutputWarning(s);
        }

        if (success >= NOMAD::SuccessType::PARTIAL_SUCCESS)
        {
            // Compute new direction for main mesh.
            // The direction is related to the frame center which generated
            // newBest.
            auto pointFromPtr = newBest->getPointFrom(); 
            auto pointNewPtr = newBest->getX();
            if (nullptr == pointFromPtr)
            {
                std::string s = "Update cannot compute new direction for successful point: pointFromPtr is NULL ";
                s += newBest->display();
                throw NOMAD::Exception(__FILE__,__LINE__, s);
            }
            // PointFrom is in full dimension. Convert it to subproblem
            // to compute direction.
            auto fixedVariable = _parentStep->getSubFixedVariable();
            auto pointFromSub = std::make_shared<NOMAD::Point>(pointFromPtr->makeSubSpacePointFromFixed(fixedVariable));

            NOMAD::Direction dir = NOMAD::Point::vectorize(*pointFromSub, *pointNewPtr);
            OUTPUT_INFO_START
            std::string dirStr = "New direction " + dir.display();
            AddOutputInfo(dirStr);
            AddOutputInfo("Last Iteration Successful.");
            OUTPUT_INFO_END
            // This computed direction will be used to sort points. Update values.
            // Use full space.
            auto pointNewFull = std::make_shared<NOMAD::Point>(pointNewPtr->makeFullSpacePointFromFixed(fixedVariable));
            NOMAD::Direction dirFull = NOMAD::Point::vectorize(*pointFromPtr, *pointNewFull);
            NOMAD::OrderByDirection::setLastSuccessfulDir(dirFull);

            // Update frame size for main mesh
            auto anisotropyFactor = _runParams->getAttributeValue<NOMAD::Double>("ANISOTROPY_FACTOR");
            bool anistropicMesh = _runParams->getAttributeValue<bool>("ANISOTROPIC_MESH");

            if (mesh->enlargeDeltaFrameSize(dir, anisotropyFactor, anistropicMesh))
            {
                OUTPUT_INFO_START
                AddOutputInfo("Delta is enlarged.");
                OUTPUT_INFO_END
            }
            else
            {
                OUTPUT_INFO_START
                AddOutputInfo("Delta is not enlarged.");
                OUTPUT_INFO_END
            }
        }
        else
        {
            OUTPUT_INFO_START
            AddOutputInfo("Last Iteration Unsuccessful. Refine Delta.");
            OUTPUT_INFO_END
            mesh->refineDeltaFrameSize();
        }
    }

    mesh->updatedeltaMeshSize();
    OUTPUT_INFO_START
    AddOutputInfo("delta mesh  size = " + mesh->getdeltaMeshSize().display());
    AddOutputInfo("Delta frame size = " + mesh->getDeltaFrameSize().display());
    OUTPUT_INFO_END


    return true;
}


