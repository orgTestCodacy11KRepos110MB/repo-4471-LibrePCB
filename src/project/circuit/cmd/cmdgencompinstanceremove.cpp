/*
 * EDA4U - Professional EDA for everyone!
 * Copyright (C) 2013 Urban Bruhin
 * http://eda4u.ubruhin.ch/
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*****************************************************************************************
 *  Includes
 ****************************************************************************************/

#include <QtCore>
#include "cmdgencompinstanceremove.h"
#include "../circuit.h"
#include "../gencompinstance.h"

namespace project {

/*****************************************************************************************
 *  Constructors / Destructor
 ****************************************************************************************/

CmdGenCompInstanceRemove::CmdGenCompInstanceRemove(Circuit& circuit,
                                                   GenCompInstance& genCompInstance,
                                                   UndoCommand* parent) throw (Exception) :
    UndoCommand(tr("Remove generic component"), parent),
    mCircuit(circuit), mGenCompInstance(genCompInstance)
{
}

CmdGenCompInstanceRemove::~CmdGenCompInstanceRemove() noexcept
{
    if (mIsExecuted)
        delete &mGenCompInstance;
}

/*****************************************************************************************
 *  Inherited from UndoCommand
 ****************************************************************************************/

void CmdGenCompInstanceRemove::redo() throw (Exception)
{
    mCircuit.removeGenCompInstance(mGenCompInstance); // throws an exception on error

    try
    {
        UndoCommand::redo(); // throws an exception on error
    }
    catch (Exception &e)
    {
        mCircuit.addGenCompInstance(mGenCompInstance);
        throw;
    }
}

void CmdGenCompInstanceRemove::undo() throw (Exception)
{
    mCircuit.addGenCompInstance(mGenCompInstance); // throws an exception on error

    try
    {
        UndoCommand::undo();
    }
    catch (Exception& e)
    {
        mCircuit.removeGenCompInstance(mGenCompInstance);
        throw;
    }
}

/*****************************************************************************************
 *  End of File
 ****************************************************************************************/

} // namespace project
