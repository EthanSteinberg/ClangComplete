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

#include <macrosmanager.h>

#include <algorithm>

#include "Result.h"


#include "myThread.h"


// Register the plugin with Code::Blocks.
// We are using an anonymous namespace so we don't litter the global one.


int onCompleteId = wxNewId();

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


    transferData* data = (transferData*)  evt.GetClientData();
    int id = data->id+1;
    cbProject *project = data->project;

    CXTranslationUnit unit = data->unit;
    wxString filename = wxString(data->filename,wxConvUTF8);
free(data);

    Manager::Get()->GetLogManager()->Log(_("Processing done for ") + filename);

    units.insert(std::make_pair(filename,unit));


    if (id< project->GetFilesCount())
    {
        InitializeTU(project->GetFile(id),id,project);

    }
    else
    {
        fileProcessed = true;
    }





}




wxString generateCommandString(ProjectFile *file)
{

    cbProject *project = file->GetParentProject();

    ProjectBuildTarget *target = project->GetBuildTarget(0);
    wxString test = target->GetCompilerID();
    Compiler * comp = CompilerFactory::GetCompiler(test);

    const pfDetails& pfd = file->GetFileDetails(target);


    wxString Object = (comp->GetSwitches().UseFlatObjects)?pfd.object_file_flat:pfd.object_file;

    const CompilerTool &tool = comp->GetCompilerTool(ctCompileObjectCmd,_(".cpp"));
    wxString tempCommand = _("$options $includes");
    comp->GenerateCommandLine(tempCommand,target,file,UnixFilename(pfd.source_file_absolute_native),Object,pfd.object_file_flat,
                              pfd.dep_file);
    return tempCommand;
}

const char** generateCommandLine(wxString command, int &numOfTokens)
{


    wxStringTokenizer tokenizer(command);
    numOfTokens = tokenizer.CountTokens();
    char const** args = new  const char*[numOfTokens];


    int i = 0;
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

wxArrayString findCompilerIncludes(Compiler* compiler)
{
    wxFileName fn(wxEmptyString, compiler->GetPrograms().CPP);
    wxString masterPath = compiler->GetMasterPath();
    Manager::Get()->GetMacrosManager()->ReplaceMacros(masterPath);
    fn.SetPath(masterPath);
    fn.AppendDir(_T("bin"));

    wxString cpp_compiler = fn.GetFullPath();

    #ifdef __WXMSW__
        wxString Command = cpp_compiler + _T(" -v -E -x c++ nul");
    #else
        wxString Command = cpp_compiler + _T(" -v -E -x c++ /dev/null");
    #endif

    wxArrayString Output,Errors;

    wxExecute(Command, Output, Errors, wxEXEC_SYNC | wxEXEC_NODISABLE);

    wxArrayString includeDirs;

    int num = Errors.GetCount();
    int i = 0;

    while ( i < num)
    {
        if (Errors[i++] == _("#include <...> search starts here:"))
            break;
    }

    while (i < num)
    {
        wxString result = Errors[i++];
        if (result == _("End of search list."))
            break;

        result.Trim(true);
        result.Trim(false);
        includeDirs.Add(_("-I") + result);

    }

    return includeDirs;


}




void ClangComplete::InitializeTU(ProjectFile *file, int id,cbProject *project)
{

//    cbProject *project = file->GetParentProject();
    ProjectBuildTarget *target = project->GetBuildTarget(0);
    Compiler *compiler = CompilerFactory::GetCompiler(target->GetCompilerID());

    wxArrayString otherIncludes = findCompilerIncludes(compiler);




    wxString name = file->file.GetFullPath();
    wxCharBuffer buffer = name.ToUTF8();


    wxString tempCommand = generateCommandString(file);
    tempCommand += GetStringFromArray(otherIncludes,_(" "));
    Manager::Get()->GetLogManager()->Log(name);
    Manager::Get()->GetLogManager()->Log(tempCommand);

    int numOfTokens;
    const char**args = generateCommandLine(tempCommand, numOfTokens);







    myThread *thread = new myThread(this,index,buffer,args,numOfTokens,id,project);
    thread->Create();
    thread->Run();


}

void ClangComplete::OnProjectOpen(CodeBlocksEvent &evt)
{


    cbProject* project = evt.GetProject();


        InitializeTU(project->GetFile(0),0,project);




}






void ClangComplete::OnEditorOpen(CodeBlocksEvent &evt)
{

    Manager::Get()->GetLogManager()->Log(_("A file has been opened ") + evt.GetEditor()->GetFilename());

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


CXCodeCompleteResults* ClangComplete::getResults(cbEditor* editor, cbStyledTextCtrl* control)
{
    wxString name = editor->GetFilename();
    wxCharBuffer buffer = name.ToUTF8();

    wxString text = control->GetText();
    wxCharBuffer textBuf = text.ToUTF8();

    int length = control->GetLength();

    CXUnsavedFile file = {buffer.data(), textBuf.data(), length};



    CXTranslationUnit unit = units[name];

    int line = control->GetCurrentLine() +1;
    int column = control->GetColumn(control->GetCurrentPos()) +2;

    int status = clang_reparseTranslationUnit(unit,1,&file, clang_defaultReparseOptions(unit));

    return clang_codeCompleteAt(unit,buffer.data(),line,column, &file, 1 , clang_defaultCodeCompleteOptions());

}


std::vector<Result> getSortedResults(CXCodeCompleteResults *results)
{
    int numResults = results->NumResults;

    std::vector<Result> sortedResults;

    for (int i = 0; i < numResults; i++)
    {

        CXCompletionResult result = results->Results[i];

        Result endResult(result);

        if (endResult.isGood())
            sortedResults.push_back(endResult);

        //Manager::Get()->GetLogManager()->Log(resulting);

    }
    clang_disposeCodeCompleteResults(results);

    std::sort(sortedResults.begin(),sortedResults.end());

    return sortedResults;

}

void showResults(const std::vector<Result> sortedResults, cbStyledTextCtrl * control)
{
    int pos   = control->GetCurrentPos();
    int start = control->WordStartPosition(pos, true);


    wxString textString = control->GetTextRange(start,pos);

    wxArrayString items;

    for (std::vector<Result>::const_iterator iter = sortedResults.begin(); iter != sortedResults.end(); iter++)
    {
        if (iter->string.StartsWith(textString))
            items.Add(iter->string);
    }


    wxString final = GetStringFromArray(items, _T("\n"));
    final.RemoveLast();



    control->AutoCompSetIgnoreCase(true);
    control->AutoCompSetCancelAtStart(true);
    control->AutoCompSetAutoHide(true);
    control->AutoCompSetSeparator('\n');
    control->AutoCompStops(_(""));
    control->AutoCompSetFillUps(_(""));


    control->AutoCompShow(pos-start,final );
    Manager::Get()->GetLogManager()->Log(textString);

}

int ClangComplete::CodeComplete()
{

    cbEditor* editor = (cbEditor*)Manager::Get()->GetEditorManager()->GetActiveEditor();
    cbStyledTextCtrl *control = editor->GetControl();

    wxString name = editor->GetFilename();
    wxCharBuffer buffer = name.ToUTF8();
    int pos   = control->GetCurrentPos();

    if (!fileProcessed || units.count(name) == 0)
    {
        control->CallTipShow(pos,_("Not finished parsing yet"));
        return 0;
    }



    for (int i = 1; i <= m_pImageList->GetImageCount(); i++)
        control->RegisterImage(i,m_pImageList->GetBitmap(i));


    const int style = control->GetStyleAt(pos);
    if (style != 0)
        return 0;


    CXCodeCompleteResults* results= getResults(editor,control);

    std::vector<Result> sortedResults = getSortedResults(results);
    showResults(sortedResults,control);


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

        int pos   = control->GetCurrentPos();
        int start = control->WordStartPosition(pos, true);



        if (ch == '.' || (ch == ':' && previousChar == ':') || (ch == '>' && previousChar == '-'))
            CodeComplete();

        else if (pos - start >= 3)
            CodeComplete();

    }
}


void ClangComplete::OnAttach()
{



    EditorHooks::HookFunctorBase* myhook = new EditorHooks::HookFunctor<ClangComplete>(this, &ClangComplete::OnStuff);
    hookId = EditorHooks::RegisterHook(myhook);


    Manager::Get()->RegisterEventSink(cbEVT_EDITOR_OPEN,          new cbEventFunctor<ClangComplete, CodeBlocksEvent>(this, &ClangComplete::OnEditorOpen));
    Manager::Get()->RegisterEventSink(cbEVT_PROJECT_ACTIVATE,          new cbEventFunctor<ClangComplete, CodeBlocksEvent>(this, &ClangComplete::OnProjectOpen));

    waitingForProject = false;

    fileProcessed = false;


    index = clang_createIndex(0,0);


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




}

void ClangComplete::OnRelease(bool appShutDown)
{
    EditorHooks::UnregisterHook(hookId, true);

    for (std::map<wxString, CXTranslationUnit>::iterator iter = units.begin(); iter != units.end(); iter++)
        clang_disposeTranslationUnit(iter->second);


        clang_disposeIndex(index);
}
