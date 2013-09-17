#pragma once

#include "Platform/System.h"

#include <wx/dnd.h>

namespace Helium
{
    namespace Editor
    {
        /////////////////////////////////////////////////////////////////////////////
        // By default, this class behaves exactly like wxDropSource.  However, if you
        // use this class, you can call the SetAutoRaise function (passing in true)
        // before you start a drag-and-drop operation.  This feature will poll the
        // mouse position during the drag-and-drop operation and if your mouse moves
        // over another top level window within the same application, that window 
        // will be brought to the front.
        // 
        class DropSource : public wxDropSource
        {
        public:
            DropSource( wxWindow* win = NULL, const wxIcon& copy = wxNullIcon, const wxIcon& move = wxNullIcon, const wxIcon& none = wxNullIcon );
            DropSource( wxDataObject& data, wxWindow* win = NULL, const wxIcon& copy = wxNullIcon, const wxIcon& move = wxNullIcon, const wxIcon& none = wxNullIcon );

            void SetAutoRaise( bool autoRaise );

            virtual wxDragResult DoDragDrop( int flags = wxDrag_CopyOnly ) HELIUM_OVERRIDE;

        private:
            bool m_AutoRaise;
        };
    }
}