#include <sdk.h> // Code::Blocks SDK
#include <configurationpanel.h>
#include "ClangComplete.h"

#include <logmanager.h>
#include <editor_hooks.h>
#include <wxscintilla/include/wx/wxscintilla.h>

#include <wx/tokenzr.h>

#include <clang-c/Index.h>
#include <cbeditor.h>
#include <cbstyledtextctrl.h>
#include <projectmanager.h>
#include <cbproject.h>
#include <compiler.h>
#include <compilerfactory.h>
#include <editormanager.h>
// Register the plugin with Code::Blocks.
// We are using an anonymous namespace so we don't litter the global one.

DECLARE_EVENT_TYPE(wxEVT_MY_EVENT, -1)

DEFINE_EVENT_TYPE(wxEVT_MY_EVENT)


void freeCommandLine(const char** args, int numOfTokens)
{

    for (int i = 0; i < numOfTokens; i++)
    {

        free((char*)args[i+1]);
    }
    free(args);


}

class myThread : public wxThread
{
cbPlugin *handle;

CXIndex index;
wxCharBuffer buffer;
wxCharBuffer textBuf;
int length;
const char** args;
int numOfTokens;

public:
    myThread(cbPlugin *cb,CXIndex _index, const  wxCharBuffer& _buffer, const wxCharBuffer& _textBuf, int _length, const char** _args, int _numOfTokens)
    {
        handle = cb;

        index = _index;
        buffer = _buffer;
        textBuf = _textBuf;
        length = _length;
        args = _args;
        numOfTokens = _numOfTokens;

    }

protected:

CXTranslationUnit threadFunc()
{
     CXUnsavedFile file = {buffer.data(), textBuf.data(), length};
    CXTranslationUnit unit = clang_parseTranslationUnit(index, buffer.data(),args,numOfTokens+1, &file,1, CXTranslationUnit_PrecompiledPreamble | CXTranslationUnit_CacheCompletionResults | CXTranslationUnit_CXXPrecompiledPreamble);
    int status = clang_reparseTranslationUnit(unit,1,&file, clang_defaultReparseOptions(unit));
    CXCodeCompleteResults* results= clang_codeCompleteAt(unit,buffer.data(),1,1, &file, 1 , clang_defaultCodeCompleteOptions());
    clang_disposeCodeCompleteResults(results);

    freeCommandLine(args,numOfTokens);

    return unit;
}


virtual ExitCode Entry()
{



   wxCommandEvent event(wxEVT_MY_EVENT, 100);
   event.SetClientData(threadFunc());
    wxPostEvent(handle,event);
    return 0;

}
};


namespace
{
PluginRegistrant<ClangComplete> reg(_T("ClangComplete"));
}

int threadDoneId = wxNewId();

BEGIN_EVENT_TABLE(ClangComplete, cbPlugin)
    EVT_COMMAND(100,wxEVT_MY_EVENT,ClangComplete::threadDone)
END_EVENT_TABLE()

void ClangComplete::threadDone(wxCommandEvent& evt)
{
    fileProcessed = true;

    unitCreated =true;
    unit = (CXTranslationUnit) evt.GetClientData();

Manager::Get()->GetLogManager()->Log(_("Processing done"));
}


wxString generateCommandString()
{
    cbEditor* editor = (cbEditor*)Manager::Get()->GetEditorManager()->GetActiveEditor();

    ProjectFile* pf = editor->GetProjectFile();
    cbProject *project = Manager::Get()->GetProjectManager()->GetActiveProject();

    ProjectBuildTarget *target = project->GetBuildTarget(0);
    wxString test = target->GetCompilerID();
    Compiler * comp = CompilerFactory::GetCompiler(test);

    const pfDetails& pfd = pf->GetFileDetails(target);


    wxString Object = (comp->GetSwitches().UseFlatObjects)?pfd.object_file_flat:pfd.object_file;

    const CompilerTool &tool = comp->GetCompilerTool(ctCompileObjectCmd,_(".cpp"));
    wxString tempCommand = _("$options $includes");
    comp->GenerateCommandLine(tempCommand,target,pf,UnixFilename(pfd.source_file_absolute_native),Object,pfd.object_file_flat,
                              pfd.dep_file);
    return tempCommand;
}

const char** generateCommandLine(wxString command,int &numOfTokens)
{
    wxStringTokenizer tokenizer(command);
    numOfTokens = tokenizer.CountTokens();
    char const** args = new  const char*[1+numOfTokens];

    args[0] = "-I/usr/lib/clang/2.9/include";

    int i = 1;
    while (tokenizer.HasMoreTokens())
    {
        wxString tokenString = tokenizer.GetNextToken();
        wxCharBuffer token= tokenString.ToUTF8();
        char* tokenData = token.data();
        wxString get;

        char* tmp = new char[tokenString.length()+1];
        memcpy(tmp,tokenData,tokenString.length()+1);
        args[i++] = tmp;
    }

    return args;

}




void ClangComplete::InitializeTU()
{

    if (unitCreated)
    {


        clang_disposeTranslationUnit(unit);
        clang_disposeIndex(index);
    }



    cbEditor* editor = (cbEditor*)Manager::Get()->GetEditorManager()->GetActiveEditor();

    wxString name = editor->GetFilename();
    wxCharBuffer buffer = name.ToUTF8();


    wxString tempCommand = generateCommandString();

    Manager::Get()->GetLogManager()->Log(name);
    Manager::Get()->GetLogManager()->Log(tempCommand);


    int numOfTokens;
    const char**args = generateCommandLine(tempCommand,numOfTokens);

    cbStyledTextCtrl* control  = editor->GetControl();




    wxString text = control->GetText();
    wxCharBuffer textBuf = text.ToUTF8();

    int length = control->GetLength();




    index = clang_createIndex(0,0);


    myThread *thread = new myThread(this,index,buffer,textBuf,length,args,numOfTokens);
    thread->Create();
    thread->Run();


}

void ClangComplete::OnProjectOpen(CodeBlocksEvent &evt)
{

    // Manager::Get()->GetLogManager()->Log(_("Project open"));
    if (waitingForProject && Manager::Get()->GetEditorManager()->GetActiveEditor() != NULL)
    {

       InitializeTU();
        waitingForProject = false;
    }

}





void ClangComplete::OnEditorOpen(CodeBlocksEvent &evt)
{

   // while (thread.IsAlive());


    if (evt.GetProject() == NULL)
    {
        waitingForProject = true;
    }
    else
       InitializeTU();
}

// constructor
ClangComplete::ClangComplete()
{
    // Make sure our resources are available.
    // In the generated boilerplate code we have no resources but when
    // we add some, it will be nice that this code is in place already ;)
    if(!Manager::LoadResource(_T("ClangComplete.zip")))
    {
        NotifyMissingFile(_T("ClangComplete.zip"));
    }


}

// destructor
ClangComplete::~ClangComplete()
{
}


void ClangComplete::OnStuff(cbEditor *editor, wxScintillaEvent& event)
{
    if (event.GetEventType() == wxEVT_SCI_CHARADDED)
    {

        const wxChar ch = event.GetKey();
        cbStyledTextCtrl* control  = editor->GetControl();
        const wxChar previousChar = control->GetCharAt(control->GetCurrentPos() -2);


        if (ch == '.' || (ch == ':' && previousChar == ':') || (ch == '>' && previousChar == '-'))
        {
            if (!fileProcessed)
            {

                Manager::Get()->GetLogManager()->Log(_("Not done"));
                return;
            }


            wxString name = editor->GetFilename();
            wxCharBuffer buffer = name.ToUTF8();

            wxString text = control->GetText();
            wxCharBuffer textBuf = text.ToUTF8();

            int length = control->GetLength();

            CXUnsavedFile file = {buffer.data(), textBuf.data(), length};




            int status = clang_reparseTranslationUnit(unit,1,&file, clang_defaultReparseOptions(unit));





            int line = control->GetCurrentLine() +1;
            int column = control->GetColumn(control->GetCurrentPos()) +2;


            CXCodeCompleteResults* results= clang_codeCompleteAt(unit,buffer.data(),line,column, &file, 1 , clang_defaultCodeCompleteOptions());

            int pos   = control->GetCurrentPos();
            int start = control->WordStartPosition(pos, true);

            wxArrayString items;




            int numResults = results->NumResults;
            clang_sortCodeCompletionResults(results->Results,results->NumResults);


            for (int i = 0; i < numResults; i++)
            {
                CXCompletionResult result = results->Results[i];
                CXCompletionString str = result.CompletionString;

                int numOfChunks = clang_getNumCompletionChunks(str);
                wxString resulting = _("");
               // resulting << clang_getCompletionPriority(str);
               // resulting << _(":");

                for (int i =0 ; i< numOfChunks; i++)
                {
                    if (clang_getCompletionChunkKind(str,i) == CXCompletionChunk_TypedText)
                    {



                    CXString str2 = clang_getCompletionChunkText(str,i);
                    const char* str3 = clang_getCString(str2);
                    resulting += wxString(str3,wxConvUTF8) ;//+ _(" ");
                    }



                }
                Manager::Get()->GetLogManager()->Log(resulting);
                items.Add(resulting);

            }
            clang_disposeCodeCompleteResults(results);

            wxString final = GetStringFromArray(items, _(" "));

            //control->CallTipShow(control->GetCurrentPos(), _("This is confusing"));
            control->AutoCompShow(pos-start,final );
            // Manager::Get()->GetLogManager()->Log(result);




        }

    }



}


void ClangComplete::OnAttach()
{
    // do whatever initialization you need for your plugin
    // NOTE: after this function, the inherited member variable
    // m_IsAttached will be TRUE...
    // You should check for it in other functions, because if it
    // is FALSE, it means that the application did *not* "load"
    // (see: does not need) this plugin...


//    Manager::GetLogManager()->Log(_("Hello?"));

//wxCommandEvent com()

//        Manager::Get()->GetEditorManager()->OnSave()
    EditorHooks::HookFunctorBase* myhook = new EditorHooks::HookFunctor<ClangComplete>(this, &ClangComplete::OnStuff);
    hookId = EditorHooks::RegisterHook(myhook);


    Manager::Get()->RegisterEventSink(cbEVT_EDITOR_OPEN,          new cbEventFunctor<ClangComplete, CodeBlocksEvent>(this, &ClangComplete::OnEditorOpen));
    Manager::Get()->RegisterEventSink(cbEVT_PROJECT_ACTIVATE,          new cbEventFunctor<ClangComplete, CodeBlocksEvent>(this, &ClangComplete::OnProjectOpen));
    unitCreated = false;
    waitingForProject = false;

    fileProcessed = false;
   // fprintf(stderr, "This is an first\n");



}

void ClangComplete::OnRelease(bool appShutDown)
{
    // do de-initialization for your plugin
    // if appShutDown is true, the plugin is unloaded because Code::Blocks is being shut down,
    // which means you must not use any of the SDK Managers
    // NOTE: after this function, the inherited member variable
    // m_IsAttached will be FALSE...
    EditorHooks::UnregisterHook(hookId, true);
    if (unitCreated)
    {


        clang_disposeTranslationUnit(unit);
        clang_disposeIndex(index);
    }
}
