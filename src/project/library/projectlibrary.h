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

#ifndef PROJECT_PROJECTLIBRARY_H
#define PROJECT_PROJECTLIBRARY_H

/*****************************************************************************************
 *  Includes
 ****************************************************************************************/

#include <QtCore>

/*****************************************************************************************
 *  Forward Declarations
 ****************************************************************************************/

class Workspace;

namespace project {
class Project;
}

namespace library {
class Symbol;
class Footprint;
class Model;
class SpiceModel;
class Package;
class GenericComponent;
class Component;
}

/*****************************************************************************************
 *  Class ProjectLibrary
 ****************************************************************************************/

namespace project {

/**
 * @brief The ProjectLibrary class
 */
class ProjectLibrary final : public QObject
{
        Q_OBJECT

    public:

        explicit ProjectLibrary(Workspace* workspace, Project* project);
        ~ProjectLibrary();

        // Getters: Library Elements
        const library::Symbol*           getSymbol(const QUuid& uuid) const;
        const library::Footprint*        getFootprint(const QUuid& uuid) const;
        const library::Model*            getModel(const QUuid& uuid) const;
        const library::SpiceModel*       getSpiceModel(const QUuid& uuid) const;
        const library::Package*          getPackage(const QUuid& uuid) const;
        const library::GenericComponent* getGenericComponent(const QUuid& uuid) const;
        const library::Component*        getComponent(const QUuid& uuid) const;

    private:

        // make some methods inaccessible...
        ProjectLibrary();
        ProjectLibrary(const ProjectLibrary& other);
        ProjectLibrary& operator=(const ProjectLibrary& rhs);

        // General
        Workspace* mWorkspace; ///< a pointer to the Workspace object (from the constructor)
        Project* mProject; ///< a pointer to the Project object (from the constructor)
        QDir mLibraryDir; ///< a QDir object that represents the "lib" directory of the project

        // The Library Elements
        QHash<QUuid, const library::Symbol*> mSymbols;
        QHash<QUuid, const library::Footprint*> mFootprints;
        QHash<QUuid, const library::Model*> mModels;
        QHash<QUuid, const library::SpiceModel*> mSpiceModels;
        QHash<QUuid, const library::Package*> mPackages;
        QHash<QUuid, const library::GenericComponent*> mGenericComponents;
        QHash<QUuid, const library::Component*> mComponents;

};

} // namespace project

#endif // PROJECT_PROJECTLIBRARY_H
