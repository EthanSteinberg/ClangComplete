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
#include <configmanager.h>
#include <wx/imaglist.h>

#include <algorithm>


// Register the plugin with Code::Blocks.
// We are using an anonymous namespace so we don't litter the global one.

static const char * cpp_keyword_xpm[] =
{
    "16 16 2 1",
    "     c None",
    ".    c #04049B",
    "                ",
    "  .......       ",
    " .........      ",
    " ..     ..      ",
    "..              ",
    "..   ..     ..  ",
    "..   ..     ..  ",
    ".. ...... ......",
    ".. ...... ......",
    "..   ..     ..  ",
    "..   ..     ..  ",
    "..      ..      ",
    "...     ..      ",
    " .........      ",
    "  .......       ",
    "                "
};






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

int threadDoneId = wxNewId();
int onCompleteId = wxNewId();
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



        wxCommandEvent event(wxEVT_MY_EVENT, threadDoneId);
        event.SetClientData(threadFunc());
        wxPostEvent(handle,event);
        return 0;

    }
};


namespace
{
PluginRegistrant<ClangComplete> reg(_T("ClangComplete"));
}


BEGIN_EVENT_TABLE(ClangComplete, cbPlugin)
    EVT_COMMAND(threadDoneId,wxEVT_MY_EVENT,ClangComplete::threadDone)
    EVT_MENU(onCompleteId, ClangComplete::OnCodeComplete)
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

wxString getImageNum(CXCursorKind kind, CX_CXXAccessSpecifier spec)
{
    wxString result = _("0");


    switch(kind)
    {
    case CXCursor_CXXMethod:
    case CXCursor_FunctionDecl:
        if (spec == CX_CXXPublic || spec == CX_CXXInvalidAccessSpecifier)
            result = _("13");

        else if (spec == CX_CXXProtected)
            result = _("12");

        else if (spec == CX_CXXPrivate)
            result = _("11");

        break;

    case CXCursor_FieldDecl:
        if (spec == CX_CXXPublic)
            result = _("16");

        else if (spec == CX_CXXProtected)
            result = _("15");

        else if (spec == CX_CXXPrivate)
            result = _("14");
        break;


    case CXCursor_EnumDecl:
        if (spec == CX_CXXPublic || spec == CX_CXXInvalidAccessSpecifier)
            result = _("21");

        else if (spec == CX_CXXProtected)
            result = _("20");

        else if (spec == CX_CXXPrivate)
            result = _("19");
        break;

    case CXCursor_EnumConstantDecl:
        result = _("22");
        break;


    case CXCursor_Constructor:
        if (spec == CX_CXXPublic || spec == CX_CXXInvalidAccessSpecifier)
            result = _("7");

        else if (spec == CX_CXXProtected)
            result = _("6");

        else if (spec == CX_CXXPrivate)
            result = _("5");
        break;

    case CXCursor_ClassDecl:
    case CXCursor_StructDecl:
        if (spec == CX_CXXPublic)
            result = _("4");

        else if (spec == CX_CXXProtected)
            result = _("3");

        else if (spec == CX_CXXPrivate)
            result = _("2");

        else if (spec == CX_CXXInvalidAccessSpecifier)
            result = _("1");
        break;

    case CXCursor_Destructor:
        if (spec == CX_CXXPublic)
            result = _("10");

        else if (spec == CX_CXXProtected)
            result = _("9");

        else if (spec == CX_CXXPrivate)
            result = _("8");
        break;

    case CXCursor_VarDecl:
        result = _("16");
        break;

    case CXCursor_MacroDefinition:
        result = _("35");
        break;

    default:
        result = _("40");
        break;

    }

    return result;

}

struct Result
{
    int rank;
    wxString string;



    bool operator<(const Result& other) const
    {
        int diff = std::abs(rank - other.rank);

        if (other.rank - rank > 1)
            return false;

        else if (diff <= 1 && string < other.string)
            return true;

        else
            return false;
    }
};

int ClangComplete::CodeComplete()
{


    if (!fileProcessed)
    {

        Manager::Get()->GetLogManager()->Log(_("Not done"));
        return 0;
    }



    cbEditor* editor = (cbEditor*)Manager::Get()->GetEditorManager()->GetActiveEditor();
    cbStyledTextCtrl *control = editor->GetControl();

    wxString name = editor->GetFilename();
    wxCharBuffer buffer = name.ToUTF8();

    wxString text = control->GetText();
    wxCharBuffer textBuf = text.ToUTF8();


    for (int i = 1; i <= m_pImageList->GetImageCount(); i++)
        control->RegisterImage(i,m_pImageList->GetBitmap(i));


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



    std::vector<Result> sortedResults;

    for (int i = 0; i < numResults; i++)
    {



        CXCompletionResult result = results->Results[i];
        CXCompletionString str = result.CompletionString;
        CXCursorKind kind = result.CursorKind;
        int blah = result.CursorAccess;

        wxString type = getImageNum(kind,result.CursorAccess);
        int numOfChunks = clang_getNumCompletionChunks(str);

        wxString resulting;


        //resulting<<blah;

        CXString spell = clang_getCursorKindSpelling(kind);
        const char* spellChar = clang_getCString(spell);
      //  wxString resulting = wxString(spellChar,wxConvUTF8);
        // resulting << clang_getCompletionPriority(str);

        if (clang_getCompletionAvailability(str) != CXAvailability_Available)
            break;
        // resulting<< clang_getCompletionAvailability(str);
        // resulting << _(":");


        for (int i =0 ; i< numOfChunks; i++)
        {
            if (true)//if (clang_getCompletionChunkKind(str,i) == CXCompletionChunk_TypedText)
            {



                CXString str2 = clang_getCompletionChunkText(str,i);
                const char* str3 = clang_getCString(str2);

                resulting += wxString(str3,wxConvUTF8);// + _("?") + type;;
               // resulting<< resulting.Length();//
               // resulting<<_(":");
                //resulting<< strlen(str3);
                clang_disposeString(str2);

                //break;
            }



        }


        if (resulting.GetChar(0) == '_')
        {

            //Manager::Get()->GetLogManager()->Log(_("It is an internal thing") + resulting);
            continue;
        }

        Result endResult = {clang_getCompletionPriority(str),resulting };
        sortedResults.push_back(endResult);

       // resulting<< clang_getCompletionPriority(str);
        Manager::Get()->GetLogManager()->Log(resulting);
        //items.Add(resulting);

    }

    std::sort(sortedResults.begin(),sortedResults.end());


    //items.Add(_("What"));
    //items.Add(_("two2"));
    int i = 0;
    for (std::vector<Result>::iterator iter = sortedResults.begin(); iter != sortedResults.end(); iter++)
    {i++;


            items.Add(iter->string);
    }


    wxArrayString foo;
    foo.Add(_("What the heck"));
    foo.Add(_("two"));
    foo.Add(_("Hello"));
    foo.Add(_("Hels"));
    foo.Add(_("Hi"));
    foo.Add(_("tuesday"));


    clang_disposeCodeCompleteResults(results);
    wxString final = GetStringFromArray(items, _("|"));
    final.RemoveLast();



    //control->CallTipShow(control->GetCurrentPos(), _("This is confusing"));
    control->AutoCompSetIgnoreCase(true);
    control->AutoCompSetCancelAtStart(false);
    control->AutoCompSetAutoHide(false);
    control->AutoCompSetSeparator('|');
    control->AutoCompStops(_(""));
   // final.RemoveLast();

//final = _("Hello boo");
    control->AutoCompShow(pos-start,final );
    wxString nums = _("pos: ");
    nums<<pos ;
    nums<<_(" start:");
    nums<<start;
     Manager::Get()->GetLogManager()->Log(final);

    return 0;
}

void ClangComplete::OnCodeComplete(wxCommandEvent &evt)
{
    CodeComplete();
}

void ClangComplete::BuildMenu(wxMenuBar *menuBar)
{


    wxMenu* menu = menuBar->GetMenu(menuBar->FindMenu(_("&Edit")));

    menu->Append(onCompleteId, _("ClangComp\tCtrl-F1"));

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
            CodeComplete();

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




    m_pImageList = new wxImageList(16, 16);
    wxBitmap bmp;

    wxString prefix = ConfigManager::GetDataFolder() + _T("/images/codecompletion/");
    // bitmaps must be added by order of PARSER_IMG_* consts
    bmp = cbLoadBitmap(prefix + _T("class_folder.png"), wxBITMAP_TYPE_PNG);
    m_pImageList->Add(bmp); // PARSER_IMG_CLASS_FOLDER
    bmp = cbLoadBitmap(prefix + _T("class.png"), wxBITMAP_TYPE_PNG);
    m_pImageList->Add(bmp); // PARSER_IMG_CLASS
    bmp = cbLoadBitmap(prefix + _T("class_private.png"), wxBITMAP_TYPE_PNG);
    m_pImageList->Add(bmp); // PARSER_IMG_CLASS_PRIVATE
    bmp = cbLoadBitmap(prefix + _T("class_protected.png"), wxBITMAP_TYPE_PNG);
    m_pImageList->Add(bmp); // PARSER_IMG_CLASS_PROTECTED
    bmp = cbLoadBitmap(prefix + _T("class_public.png"), wxBITMAP_TYPE_PNG);
    m_pImageList->Add(bmp); // PARSER_IMG_CLASS_PUBLIC
    bmp = cbLoadBitmap(prefix + _T("ctor_private.png"), wxBITMAP_TYPE_PNG);
    m_pImageList->Add(bmp); // PARSER_IMG_CTOR_PRIVATE
    bmp = cbLoadBitmap(prefix + _T("ctor_protected.png"), wxBITMAP_TYPE_PNG);
    m_pImageList->Add(bmp); // PARSER_IMG_CTOR_PROTECTED
    bmp = cbLoadBitmap(prefix + _T("ctor_public.png"), wxBITMAP_TYPE_PNG);
    m_pImageList->Add(bmp); // PARSER_IMG_CTOR_PUBLIC
    bmp = cbLoadBitmap(prefix + _T("dtor_private.png"), wxBITMAP_TYPE_PNG);
    m_pImageList->Add(bmp); // PARSER_IMG_DTOR_PRIVATE
    bmp = cbLoadBitmap(prefix + _T("dtor_protected.png"), wxBITMAP_TYPE_PNG);
    m_pImageList->Add(bmp); // PARSER_IMG_DTOR_PROTECTED
    bmp = cbLoadBitmap(prefix + _T("dtor_public.png"), wxBITMAP_TYPE_PNG);
    m_pImageList->Add(bmp); // PARSER_IMG_DTOR_PUBLIC
    bmp = cbLoadBitmap(prefix + _T("method_private.png"), wxBITMAP_TYPE_PNG);
    m_pImageList->Add(bmp); // PARSER_IMG_FUNC_PRIVATE
    bmp = cbLoadBitmap(prefix + _T("method_protected.png"), wxBITMAP_TYPE_PNG);
    m_pImageList->Add(bmp); // PARSER_IMG_FUNC_PRIVATE
    bmp = cbLoadBitmap(prefix + _T("method_public.png"), wxBITMAP_TYPE_PNG);
    m_pImageList->Add(bmp); // PARSER_IMG_FUNC_PUBLIC
    bmp = cbLoadBitmap(prefix + _T("var_private.png"), wxBITMAP_TYPE_PNG);
    m_pImageList->Add(bmp); // PARSER_IMG_VAR_PRIVATE
    bmp = cbLoadBitmap(prefix + _T("var_protected.png"), wxBITMAP_TYPE_PNG);
    m_pImageList->Add(bmp); // PARSER_IMG_VAR_PROTECTED
    bmp = cbLoadBitmap(prefix + _T("var_public.png"), wxBITMAP_TYPE_PNG);
    m_pImageList->Add(bmp); // PARSER_IMG_VAR_PUBLIC
    bmp = cbLoadBitmap(prefix + _T("preproc.png"), wxBITMAP_TYPE_PNG);
    m_pImageList->Add(bmp); // PARSER_IMG_PREPROCESSOR
    bmp = cbLoadBitmap(prefix + _T("enum.png"), wxBITMAP_TYPE_PNG);
    m_pImageList->Add(bmp); // PARSER_IMG_ENUM
    bmp = cbLoadBitmap(prefix + _T("enum_private.png"), wxBITMAP_TYPE_PNG);
    m_pImageList->Add(bmp); // PARSER_IMG_ENUM_PRIVATE
    bmp = cbLoadBitmap(prefix + _T("enum_protected.png"), wxBITMAP_TYPE_PNG);
    m_pImageList->Add(bmp); // PARSER_IMG_ENUM_PROTECTED
    bmp = cbLoadBitmap(prefix + _T("enum_public.png"), wxBITMAP_TYPE_PNG);
    m_pImageList->Add(bmp); // PARSER_IMG_ENUM_PUBLIC
    bmp = cbLoadBitmap(prefix + _T("enumerator.png"), wxBITMAP_TYPE_PNG);
    m_pImageList->Add(bmp); // PARSER_IMG_ENUMERATOR
    bmp = cbLoadBitmap(prefix + _T("namespace.png"), wxBITMAP_TYPE_PNG);
    m_pImageList->Add(bmp); // PARSER_IMG_NAMESPACE
    bmp = cbLoadBitmap(prefix + _T("typedef.png"), wxBITMAP_TYPE_PNG);
    m_pImageList->Add(bmp); // PARSER_IMG_TYPEDEF
    bmp = cbLoadBitmap(prefix + _T("typedef_private.png"), wxBITMAP_TYPE_PNG);
    m_pImageList->Add(bmp); // PARSER_IMG_TYPEDEF_PRIVATE
    bmp = cbLoadBitmap(prefix + _T("typedef_protected.png"), wxBITMAP_TYPE_PNG);
    m_pImageList->Add(bmp); // PARSER_IMG_TYPEDEF_PROTECTED
    bmp = cbLoadBitmap(prefix + _T("typedef_public.png"), wxBITMAP_TYPE_PNG);
    m_pImageList->Add(bmp); // PARSER_IMG_TYPEDEF_PUBLIC
    bmp = cbLoadBitmap(prefix + _T("symbols_folder.png"), wxBITMAP_TYPE_PNG);
    m_pImageList->Add(bmp); // PARSER_IMG_SYMBOLS_FOLDER
    bmp = cbLoadBitmap(prefix + _T("vars_folder.png"), wxBITMAP_TYPE_PNG);
    m_pImageList->Add(bmp); // PARSER_IMG_VARS_FOLDER
    bmp = cbLoadBitmap(prefix + _T("funcs_folder.png"), wxBITMAP_TYPE_PNG);
    m_pImageList->Add(bmp); // PARSER_IMG_FUNCS_FOLDER
    bmp = cbLoadBitmap(prefix + _T("enums_folder.png"), wxBITMAP_TYPE_PNG);
    m_pImageList->Add(bmp); // PARSER_IMG_ENUMS_FOLDER
    bmp = cbLoadBitmap(prefix + _T("preproc_folder.png"), wxBITMAP_TYPE_PNG);
    m_pImageList->Add(bmp); // PARSER_IMG_PREPROC_FOLDER
    bmp = cbLoadBitmap(prefix + _T("others_folder.png"), wxBITMAP_TYPE_PNG);
    m_pImageList->Add(bmp); // PARSER_IMG_OTHERS_FOLDER
    bmp = cbLoadBitmap(prefix + _T("typedefs_folder.png"), wxBITMAP_TYPE_PNG);
    m_pImageList->Add(bmp); // PARSER_IMG_TYPEDEF_FOLDER
    bmp = cbLoadBitmap(prefix + _T("macro.png"), wxBITMAP_TYPE_PNG);
    m_pImageList->Add(bmp); // PARSER_IMG_MACRO
    bmp = cbLoadBitmap(prefix + _T("macro_private.png"), wxBITMAP_TYPE_PNG);
    m_pImageList->Add(bmp); // PARSER_IMG_MACRO_PRIVATE
    bmp = cbLoadBitmap(prefix + _T("macro_protected.png"), wxBITMAP_TYPE_PNG);
    m_pImageList->Add(bmp); // PARSER_IMG_MACRO_PROTECTED
    bmp = cbLoadBitmap(prefix + _T("macro_public.png"), wxBITMAP_TYPE_PNG);
    m_pImageList->Add(bmp); // PARSER_IMG_MACRO_PUBLIC
    bmp = cbLoadBitmap(prefix + _T("macro_folder.png"), wxBITMAP_TYPE_PNG);
    m_pImageList->Add(bmp); // PARSER_IMG_MACRO_FOLDER
    bmp = wxImage(cpp_keyword_xpm);
    m_pImageList->Add(bmp);



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
