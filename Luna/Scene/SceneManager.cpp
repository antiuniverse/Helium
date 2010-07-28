#include "Precompile.h"
#include "SceneManager.h"

#include "Scene.h"
#include "SceneEditor.h"
#include "SceneDocument.h"
#include "SwitchSceneCommand.h"
#include "Viewport.h"

#include "Pipeline/Asset/Classes/SceneAsset.h"
#include "Foundation/Component/ComponentHandle.h"
#include "Foundation/Container/Insert.h" 
#include "Foundation/Log.h"
#include "Application/UI/FileDialog.h"

// Using
using namespace Luna;

///////////////////////////////////////////////////////////////////////////////
// Returns a different name each time this function is called so that scenes
// can be uniquely named.
// 
static tstring GetUniqueSceneName()
{
    static i32 number = 1;

    tostringstream str;
    str << "Scene" << number++;
    return str.str();
}

///////////////////////////////////////////////////////////////////////////////
// 
// 
#ifdef UI_REFACTOR
SceneManager::SceneManager(SceneEditor* editor)
: DocumentManager( editor )
#else
SceneManager::SceneManager()
: DocumentManager()
#endif
, m_CurrentScene( NULL )
#ifdef UI_REFACTOR
, m_Editor (editor)
#endif
{

}

///////////////////////////////////////////////////////////////////////////////
// 
// 
SceneManager::~SceneManager()
{

}

#ifdef UI_REFACTOR
///////////////////////////////////////////////////////////////////////////////
// The editor that owns us
// 
SceneEditor* SceneManager::GetEditor()
{
    return m_Editor;
}
#endif

///////////////////////////////////////////////////////////////////////////////
// Create a new scene.  Pass in true if this should be the root scene.
// 
ScenePtr SceneManager::NewScene( Luna::Viewport* viewport, bool isRoot, tstring path, bool addDoc )
{
    tstring name;
    if ( path.empty() )
    {
        name = GetUniqueSceneName();
    }

    SceneDocumentPtr document = new SceneDocument( path, name );
    document->AddDocumentClosedListener( DocumentChangedSignature::Delegate( this, &SceneManager::OnDocumentClosed ) );
    ScenePtr scene = new Luna::Scene( viewport, this, document );
    if ( isRoot )
    {
        SetRootScene( scene );
    }
    AddScene( scene );

    if ( addDoc && !AddDocument( document ) )
    {
        // Shouldn't happen
        NOC_BREAK();
    }
    return scene;
}

///////////////////////////////////////////////////////////////////////////////
// Open a top-level scene (world).
// 
DocumentPtr SceneManager::OpenPath( const tstring& path, tstring& error ) 
{
    if ( !CloseAll() )
    {
        return NULL;
    }

    // Create a batch command to toggle between the previous scene, the new scene,
    // and no scene (while we are loading the file).  This batch command will be
    // committed to the undo stack only if this is not the first scene that we
    // are opening.
    Undo::BatchCommandPtr batch = new Undo::BatchCommand();
    Luna::Scene* previousScene = GetCurrentScene();
    if ( previousScene )
    {
        batch->Push( new LSwitchSceneCommand( this, NULL ) );
    }

    tstring scenePath = path;
    SceneDocumentPtr document;
    Nocturnal::Path filePath( path );

    ScenePtr scene = NewScene( NULL, m_Root == NULL, scenePath, true );
    if ( !scene->LoadFile( scenePath ) )
    {
        error = TXT( "Failed to load scene from " ) + path + TXT( "." );
        RemoveScene( scene );
        scene = NULL;
    }

    if ( scene.ReferencesObject() )
    {
        document = scene->GetSceneDocument();
        if ( !m_CurrentScene )
        {
            SetCurrentScene( scene );
        }

        batch->Push( new LSwitchSceneCommand( this, scene ) );
        if ( previousScene && GetRootScene() )
        {
            GetRootScene()->Push( batch );
        }
        // else: Throw away the batch;  it's already done the work of switching scenes
        // and we didn't start with a valid scene, so this command is meaningless to put
        // on the undo queue.
    }
    else
    {
        // If we switched from a valid scene, restore that scene as the current one
        if ( previousScene )
        {
            batch->Push( new LSwitchSceneCommand( this, previousScene ) );
        }
    }
    return document;
}

///////////////////////////////////////////////////////////////////////////////
// Open a zone that should be under the root.
// 
ScenePtr SceneManager::OpenScene( Luna::Viewport* viewport, const tstring& path, tstring& error )
{
    ScenePtr scene = NewScene( viewport, false, path, true );
    if ( !scene->LoadFile( path ) )
    {
        error = TXT( "Failed to load scene from " ) + path + TXT( "." );
        RemoveScene( scene );
        scene = NULL;
    }

    return scene;
}

///////////////////////////////////////////////////////////////////////////////
// Prompt the user to save a file to a new location.  Returns the path to the
// new file location, or an empty string if the user cancels the operation.
// 
static tstring PromptSaveAs( const DocumentPtr& file, wxWindow* window = NULL )
{
    tstring path;
    tstring defaultDir = Nocturnal::Path( file->GetFilePath() ).Directory();
    tstring defaultFile = file->GetFilePath();

    Nocturnal::FileDialog saveDlg( window, TXT( "Save As..." ), defaultDir.c_str(), defaultFile.c_str(), TXT( "" ), Nocturnal::FileDialogStyles::DefaultSave );
    
    std::set< tstring > extensions;
    Reflect::Archive::GetExtensions( extensions );
    for ( std::set< tstring >::const_iterator itr = extensions.begin(), end = extensions.end(); itr != end; ++itr )
    {
        saveDlg.AddFilter( TXT( "Scene (*.scene." ) + *itr + TXT( ")|*.scene." ) + *itr );
    }

    if ( saveDlg.ShowModal() == wxID_OK )
    {
        path = saveDlg.GetPath();
    }

    return path;
}

///////////////////////////////////////////////////////////////////////////////
// Called when the "save all" option is chosen in the UI.  Iterates over all
// the open scenes and asks the session manager to save them.
// 
bool SceneManager::Save( DocumentPtr document, tstring& error )
{
    SceneDocument* sceneDocument = Reflect::ObjectCast< SceneDocument >( document );
    if ( !sceneDocument )
    {
        NOC_BREAK();
        error = document->GetFilePath() + TXT( " is not a valid scene file." );
        return false;
    }

    Luna::Scene* scene = sceneDocument->GetScene();
    if ( !scene )
    {
        NOC_BREAK();
        error = scene->GetFullPath() + TXT( " does not contain a valid scene to save." );
        return false;
    }

    // Check for "save as"
    if ( document->GetFilePath().empty() )
    {
#ifdef UI_REFACTOR
        tstring savePath = PromptSaveAs( sceneDocument, m_Editor );
#else
        tstring savePath = PromptSaveAs( sceneDocument );
#endif
        if ( !savePath.empty() )
        {
            document->SetFilePath( savePath );
        }
        else
        {
            // No error, operation cancelled
            return true;
        }
    }

    if ( scene->Save() )
    {
        return __super::Save( document, error );
    }

    error = TXT( "Failed to save " ) + scene->GetFullPath();
    return false;
}

///////////////////////////////////////////////////////////////////////////////
// Sets the root scene.  There can only be one root scene.  All zones that are
// created should be added to the root scene.
// 
void SceneManager::SetRootScene( Luna::Scene* root )
{
    if ( m_Root.Ptr() != root )
    {
        if ( m_Root.Ptr() )
        {
            // Do something?  Close all open scenes?
        }

        m_Root = root;

        if ( m_Root.Ptr() )
        {
            // Do something?  Notify listeners?
        }
    }
}

///////////////////////////////////////////////////////////////////////////////
// Returns the root scene.
// 
Luna::Scene* SceneManager::GetRootScene()
{
    return m_Root;
}

///////////////////////////////////////////////////////////////////////////////
// Returns true if the specified scene is the root scene.
// 
bool SceneManager::IsRoot( Luna::Scene* scene ) const
{
    return ( ( scene != NULL ) && ( scene == m_Root ) );
}

///////////////////////////////////////////////////////////////////////////////
// Adds a scene to this manager
// 
void SceneManager::AddScene(Luna::Scene* scene)
{
    scene->GetSceneDocument()->AddDocumentPathChangedListener( DocumentPathChangedSignature::Delegate ( this, &SceneManager::DocumentPathChanged ) );

    const tstring& path = scene->GetFullPath();
    Nocturnal::Insert<M_SceneSmartPtr>::Result inserted = m_Scenes.insert( M_SceneSmartPtr::value_type( path, scene ) );
    NOC_ASSERT(inserted.second);

    m_SceneAdded.Raise( scene );
}

///////////////////////////////////////////////////////////////////////////////
// Removes a scene from this manager
// 
void SceneManager::RemoveScene(Luna::Scene* scene)
{
    // There is a problem in the code.  You should not be unloading a scene that
    // someone still has allocated.
    NOC_ASSERT( m_AllocatedScenes.find( scene ) == m_AllocatedScenes.end() );

    // RemoveScene is called for nested scenes and non-nested
    // we don't SaveVisibility in remove scene because i don't want to save it for nested scenes
    // and because we remove from m_AllocatedScenes before calling RemoveScene, 
    // we have no way to test (in this function) that it was a nested scene we're unloading. 
    // 

    scene->GetSceneDocument()->RemoveDocumentPathChangedListener( DocumentPathChangedSignature::Delegate ( this, &SceneManager::DocumentPathChanged ) );
    m_SceneRemoving.Raise( scene );

    M_SceneSmartPtr::iterator found = m_Scenes.find( scene->GetFullPath() );
    NOC_ASSERT( found != m_Scenes.end() );

    if (found->second.Ptr() == m_CurrentScene)
    {
        if (m_Scenes.size() <= 1)
        {
            SetCurrentScene(NULL);
        }
        else
        {
            SetCurrentScene( FindFirstNonNestedScene() ); 
        }
    }

    if ( IsRoot( scene ) )
    {
        SetRootScene( NULL );
    }

    RemoveDocument( scene->GetSceneDocument() );
    m_Scenes.erase( found );
}

///////////////////////////////////////////////////////////////////////////////
// Removes all the scenes from the scene manager.
// 
void SceneManager::RemoveAllScenes()
{
    SetCurrentScene( NULL );

#pragma TODO("This is a hack to support our current unwinding of all the allocated scenes, which is unordered.  We need to redesign the scene manager so that the root scene is deallocated last")
    ScenePtr root = m_Root; // hold a reference to the root while we close all its nested scenes

    typedef std::vector< Luna::Scene* > V_SceneDumbPtr;
    V_SceneDumbPtr topLevelScenes;

    M_SceneSmartPtr::const_iterator sceneItr = m_Scenes.begin();
    M_SceneSmartPtr::const_iterator sceneEnd = m_Scenes.end();
    for ( ; sceneItr != sceneEnd; ++sceneItr )
    {
        Luna::Scene* scene = sceneItr->second;
        if ( m_AllocatedScenes.find( scene ) == m_AllocatedScenes.end() )
        {
            topLevelScenes.push_back( scene );
        }
    }

    V_SceneDumbPtr::const_iterator removeItr = topLevelScenes.begin();
    V_SceneDumbPtr::const_iterator removeEnd = topLevelScenes.end();
    for ( ; removeItr != removeEnd; ++removeItr )
    {
        (*removeItr)->SaveVisibility(); 
        RemoveScene( *removeItr );
    }

    root = NULL;
}

///////////////////////////////////////////////////////////////////////////////
// Returns the full list of scenes (includes nested scenes).
// 
const M_SceneSmartPtr& SceneManager::GetScenes() const 
{
    return m_Scenes;
}

///////////////////////////////////////////////////////////////////////////////
// Finds a scene by full path in this manager
// 
Luna::Scene* SceneManager::GetScene( const tstring& path ) const
{
    M_SceneSmartPtr::const_iterator found = m_Scenes.find( path );

    if (found != m_Scenes.end())
    {
        return found->second.Ptr();
    }

    return NULL;
}

///////////////////////////////////////////////////////////////////////////////
// Returns true if the specified scene is a nested (allocated) scene.  Otherwise
// the scene is a world or zone.
// 
bool SceneManager::IsNestedScene( Luna::Scene* scene ) const
{
    return m_AllocatedScenes.find( scene ) != m_AllocatedScenes.end();
}

///////////////////////////////////////////////////////////////////////////////
// Returns the specified scene and increases the reference count on that scene.
// If the scene was not yet created, this function will create a new scene.  If
// there was a problem loading the scene, it will be empty.  If you allocate a
// scene, you must call ReleaseNestedScene to free it.
// 
Luna::Scene* SceneManager::AllocateNestedScene( Luna::Viewport* viewport, const tstring& path, Luna::Scene* parent )
{
    Luna::Scene* scene = GetScene( path );

    if ( !scene )
    {
        // Try to load nested scene.
        parent->ChangeStatus( TXT("Loading ") + path + TXT( "..." ) );

        ScenePtr scenePtr = NewScene( viewport, false, path, false );
        if ( !scenePtr->LoadFile( path ) )
        {
            Log::Error( TXT( "Failed to load scene from %s\n" ), path.c_str() );
        }

        parent->ChangeStatus( TXT( "Ready" ) );
        scene = scenePtr;
    }

    if ( scene )
    {
        // Increment the reference count on the nested scene.
        i32& referenceCount = m_AllocatedScenes.insert( M_AllocScene::value_type( scene, 0 ) ).first->second;
        ++referenceCount;
    }

    return scene;
}

///////////////////////////////////////////////////////////////////////////////
// Decrements the reference count on the specified scene.  If the reference 
// count reaches zero, the scene will be removed from the manager.
// 
void SceneManager::ReleaseNestedScene( Luna::Scene*& scene )
{
    M_AllocScene::iterator found = m_AllocatedScenes.find( scene );
    if ( found != m_AllocatedScenes.end() )
    {
        i32& referenceCount = found->second;
        if ( --referenceCount == 0 )
        {
            m_AllocatedScenes.erase( found );
            RemoveScene( scene );
        }
    }
    else
    {
        // You tried to release a scene that was not allocated
        NOC_BREAK();
    }

    scene = NULL;
}

///////////////////////////////////////////////////////////////////////////////
// Returns true if there is currently a valid scene that has focus for any
// edits.
// 
bool SceneManager::HasCurrentScene() const
{
    return m_CurrentScene != NULL;
}

///////////////////////////////////////////////////////////////////////////////
// Returns true if the scene that is passed in is the same as the current
// "editing scene".
// 
bool SceneManager::IsCurrentScene( const Luna::Scene* sceneToCompare ) const
{
    return m_CurrentScene == sceneToCompare;
}

///////////////////////////////////////////////////////////////////////////////
// "Gets" the "Current Scene"
// 
Luna::Scene* SceneManager::GetCurrentScene() const
{
    return m_CurrentScene;
}

///////////////////////////////////////////////////////////////////////////////
// Freezes sorting in all the outliners (tree controls).
// 
void SceneManager::FreezeTreeSorting()
{
#ifdef UI_REFACTOR
    m_Editor->GetTreeMonitor().FreezeSorting();
#endif
}

///////////////////////////////////////////////////////////////////////////////
// Resumes sorting (and sorts) all the outliners.
// 
void SceneManager::ThawTreeSorting()
{
#ifdef UI_REFACTOR
    m_Editor->GetTreeMonitor().ThawSorting();
#endif
}

///////////////////////////////////////////////////////////////////////////////
// Only one scene can be the active, editing one.  Any new items created, selcted,
// etc., will all interact with the current "editing scene." Use this function to 
// change which scene is being edited.
// 
void SceneManager::SetCurrentScene( Luna::Scene* scene )
{
    if ( m_CurrentScene == scene )
    {
        return;
    }

    m_CurrentSceneChanging.Raise( m_CurrentScene );

    m_CurrentScene = scene;

    m_CurrentSceneChanged.Raise( m_CurrentScene );
}

///////////////////////////////////////////////////////////////////////////////
// Returns true if there is a command on the undo stack.
// 
bool SceneManager::CanUndo()
{
    return m_UndoManager.CanUndo();
}

///////////////////////////////////////////////////////////////////////////////
// Returns true if there is a command on the redo stack.
// 
bool SceneManager::CanRedo()
{
    return m_UndoManager.CanRedo();
}

///////////////////////////////////////////////////////////////////////////////
// Undo the previous command.
// 
void SceneManager::Undo()
{
    m_UndoManager.Undo();
}

///////////////////////////////////////////////////////////////////////////////
// Redo the previously undone command.
// 
void SceneManager::Redo()
{
    m_UndoManager.Redo();
}

///////////////////////////////////////////////////////////////////////////////
// The scene manager keeps track of which scene's undo queue contains the last
// command that was done.  For every command done on a scene, this Push call
// should be made to keep the scene manager up to date.  Adds the specified 
// undo queue to be the next undoable operation.
// 
void SceneManager::Push( Undo::Queue* queue )
{
    m_UndoManager.Push( queue );
}

///////////////////////////////////////////////////////////////////////////////
// Iterates over the scenes and returns the first one that is not allocated
// (i.e. not a nested scene).
// 
Luna::Scene* SceneManager::FindFirstNonNestedScene() const
{
    Luna::Scene* found = NULL;
    M_SceneSmartPtr::const_iterator sceneItr = m_Scenes.begin();
    M_SceneSmartPtr::const_iterator sceneEnd = m_Scenes.end();
    const M_AllocScene::const_iterator nestedSceneEnd = m_AllocatedScenes.end();
    for ( ; sceneItr != sceneEnd && !found; ++sceneItr )
    {
        Luna::Scene* current = sceneItr->second;
        if ( m_AllocatedScenes.find( current ) == nestedSceneEnd )
        {
            found = current; // breaks out of loop
        }
    }

    return found;
}

///////////////////////////////////////////////////////////////////////////////
// Callback for when the path of a scene changes.  Since the scene manager 
// stores the scenes by their paths, the internal list has to be updated.
// 
void SceneManager::DocumentPathChanged( const DocumentPathChangedArgs& args )
{
    const tstring pathOrName = !args.m_OldFilePath.empty() ? args.m_OldFilePath : args.m_OldFileName;
    M_SceneSmartPtr::iterator found = m_Scenes.find( pathOrName );
    if ( found != m_Scenes.end() )
    {
        // Hold a reference to the scene while we re-add it to the list, otherwise
        // it will get deleted.
        ScenePtr scene = found->second;

        m_Scenes.erase( found );
        Nocturnal::Insert<M_SceneSmartPtr>::Result inserted = 
            m_Scenes.insert( M_SceneSmartPtr::value_type( scene->GetFullPath(), scene ) );
        NOC_ASSERT( inserted.second );
    }
}

///////////////////////////////////////////////////////////////////////////////
// Callback for when a document is closed.  Closes the associated scene.
// 
void SceneManager::OnDocumentClosed( const DocumentChangedArgs& args )
{
    const SceneDocument* document = Reflect::ConstObjectCast< SceneDocument >( args.m_Document );
    NOC_ASSERT( document );

#ifdef UI_REFACTOR
    m_Editor->SyncPropertyThread();
#endif

    if ( document )
    {
        ScenePtr scene = document->GetScene();

        // If the current scene is the one that is being closed, we need to set it
        // to no longer be the current scene.
        if ( HasCurrentScene() && GetCurrentScene() == scene )
        {
            SetCurrentScene( NULL );
        }

        if ( IsRoot( scene ) )
        {
            RemoveAllScenes();
        }
        else
        {
            scene->SaveVisibility(); 
            RemoveScene( scene );
        }

        // Select the next scene in the list, if there is one
        if ( !HasCurrentScene() )
        {
            SetCurrentScene( FindFirstNonNestedScene() );
        }

        document->RemoveDocumentClosedListener( DocumentChangedSignature::Delegate( this, &SceneManager::OnDocumentClosed ) );
    }
}