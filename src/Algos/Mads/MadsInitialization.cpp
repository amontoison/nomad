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
#include "../../Algos/EvcInterface.hpp"

#include "../../Algos/Mads/GMesh.hpp"
#include "../../Algos/Mads/MadsInitialization.hpp"


void NOMAD::MadsInitialization::init()
{
    _name = NOMAD::Initialization::getName();
}


bool NOMAD::MadsInitialization::runImp()
{
    bool doContinue = ! _stopReasons->checkTerminate();

    if (doContinue)
    {
        eval_x0s();
        doContinue = ! _stopReasons->checkTerminate();
    }
    return doContinue;
}


void NOMAD::MadsInitialization::validateX0s() const
{
    auto x0s = _pbParams->getAttributeValue<NOMAD::ArrayOfPoint>("X0");
    size_t n = _pbParams->getAttributeValue<size_t>("DIMENSION");
    bool validX0available = false;
    std::string err;

    for (size_t x0index = 0; x0index < x0s.size(); x0index++)
    {
        auto x0 = x0s[x0index];
        if (!x0.isComplete() || x0.size() != n)
        {
            err += "Initialization: eval_x0s: Invalid X0 " + x0.display() + ".";
        }
        else
        {
            validX0available = true;
        }
    }
    if (validX0available)
    {
        if (!err.empty())
        {
            // Show invalid X0s
            AddOutputWarning(err);
        }
    }
    else
    {
        // No valid X0 available. Throw exception.
        size_t cacheSize = NOMAD::CacheBase::getInstance()->size();
        if (cacheSize > 0)
        {
            err += " Hint: Try not setting X0 so that the cache is used (";
            err += std::to_string(cacheSize) + " points)";
        }
        else
        {
            err += ". Cache is empty.";
        }
        throw NOMAD::Exception(__FILE__, __LINE__, err);
    }

}


bool NOMAD::MadsInitialization::eval_x0s()
{
    bool evalOk = false;
    std::string s;

    auto x0s = _pbParams->getAttributeValue<NOMAD::ArrayOfPoint>("X0");

    validateX0s();
    auto evalType = getEvalType();

    // Add X0s that need evaluation to eval queue
    NOMAD::CacheInterface cacheInterface(this);
    NOMAD::EvcInterface evcInterface(this);
    evcInterface.getEvaluatorControl()->lockQueue();

    NOMAD::EvalPointSet evalPointSet;
    for (size_t x0index = 0; x0index < x0s.size(); x0index++)
    {
        auto x0 = x0s[x0index];
        NOMAD::EvalPoint evalPointX0(x0);
        evalPointSet.insert(evalPointX0);
    }

    // Add points to the eval queue.
    // Convert to full dimension if needed.
    // Note: Queue is already locked - it needs to be locked to add points.
    evcInterface.keepPointsThatNeedEval(evalPointSet, false);   // false: no mesh

    // Enforce no opportunism.
    auto evcParams = evcInterface.getEvaluatorControl()->getEvaluatorControlParams();
    auto previousOpportunism = evcParams->getAttributeValue<bool>("OPPORTUNISTIC_EVAL");
    evcParams->setAttributeValue("OPPORTUNISTIC_EVAL", false);
    evcParams->checkAndComply();

    evcInterface.getEvaluatorControl()->unlockQueue(false); // false: do not sort eval queue

    // Evaluate all x0s. Ignore returned success type.
    // Note: EvaluatorControl would not be able to compare/compute success since there is no barrier.
    evcInterface.startEvaluation();

    // Reset opportunism to previous values.
    evcInterface.getEvaluatorControl()->lockQueue();
    evcParams->setAttributeValue("OPPORTUNISTIC_EVAL", previousOpportunism);
    evcParams->checkAndComply();
    evcInterface.getEvaluatorControl()->unlockQueue(false); // false: do not sort eval queue

    bool x0Failed = true;

    // Construct barrier using points evaluated by this step.
    // The points are cleared from the EvaluatorControl.
    auto evaluatedPoints = evcInterface.getAllEvaluatedPoints();
    std::vector<NOMAD::EvalPoint> evalPointX0s;
    for (auto x0 : x0s)
    {
        NOMAD::EvalPoint evalPointX0(x0);

        // Look for x0 in freshly evaluated points
        bool x0Found = findInList(x0, evaluatedPoints, evalPointX0);

        if (!x0Found)
        {
            auto barrier = NOMAD::EvcInterface::getEvaluatorControl()->getBarrier();
            if (nullptr != barrier)
            {
                // Look for x0 in EvaluatorControl barrier
                x0Found = findInList(x0, barrier->getAllPoints(), evalPointX0);
            }
            if (!x0Found && NOMAD::EvcInterface::getEvaluatorControl()->getUseCache())
            {
                // Look for x0 in cache
                x0Found = (cacheInterface.find(x0, evalPointX0) > 0);
            }
        }

        if (x0Found && evalPointX0.isEvalOk(evalType))
        {
            // evalOk is true if at least one evaluation is Ok
            evalOk = true;
            evalPointX0s.push_back(evalPointX0);

            x0Failed = false;   // At least one good X0.
        }
    }

    if (x0Failed)
    {
        // All x0s failed. Show an error.
        auto madsStopReason = NOMAD::AlgoStopReasons<NOMAD::MadsStopType>::get(_stopReasons);
        madsStopReason->set(NOMAD::MadsStopType::X0_FAIL);

        for (auto x0 : x0s)
        {
            AddOutputError("X0 evaluation failed for X0 = " + x0.display());
        }
    }
    else
    {
        OUTPUT_INFO_START
        for (auto evalPointX0 : evalPointX0s)
        {
            s = "Using X0: ";
            // BB: Simple display. SGTE: Full display.
            s += (NOMAD::EvalType::BB == evalType) ? evalPointX0.display() : evalPointX0.displayAll();
        }
        AddOutputInfo(s);
        OUTPUT_INFO_END

        // Construct barrier using x0s
        auto hMax = _runParams->getAttributeValue<NOMAD::Double>("H_MAX_0");
        _barrier = std::make_shared<NOMAD::Barrier>(hMax, getSubFixedVariable(), getEvalType(), evalPointX0s);
    }

    NOMAD::OutputQueue::Flush();

    return evalOk;
}
