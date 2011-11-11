#include <sdk.h> // Code::Blocks SDK
#include <configurationpanel.h>
#include "ClangComplete.h"

#include <logmanager.h>
#include <editor_hooks.h>
#include <wxscintilla/include/wx/wxscintilla.h>

#include <clang-c/Index.h>
#include <cbeditor.h>
#include <cbstyledtextctrl.h>
#include <projectmanager.h>
#include <cbproject.h>
#include <compiler.h>
#include <compilerfactory.h>
// Register the plugin with Code::Blocks.
// We are using an anonymous namespace so we don't litter the global one.
namespace
{
PluginRegistrant<ClangComplete> reg(_T("ClangComplete"));
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
//
//    if (event.GetEventType() == wxEVT_SCI_CHARADDED)
//        Manager::Get()->GetLogManager()->Log(_("wxEVT_SCI_CHARADDED"));
//    else if (event.GetEventType() == wxEVT_SCI_CHANGE)
//        Manager::Get()->GetLogManager()->Log(_("wxEVT_SCI_CHANGE"));
//    else if (event.GetEventType() == wxEVT_SCI_KEY)
//        Manager::Get()->GetLogManager()->Log(_("wxEVT_SCI_KEY"));
//    else if (event.GetEventType() == wxEVT_SCI_MODIFIED)
//        Manager::Get()->GetLogManager()->Log(_("wxEVT_SCI_MODIFIED"));


    if (event.GetEventType() == wxEVT_SCI_CHARADDED)
    {

        static wxChar lastKey = _T(' ');
        const wxChar ch = event.GetKey();
        if (ch == '.' || (ch == ':' && lastKey == ':'))
        {
            cbStyledTextCtrl* control  = editor->GetControl();

            wxString name = editor->GetFilename();
            wxCharBuffer buffer = name.ToUTF8();


            wxString text = control->GetText();
            wxCharBuffer textBuf = text.ToUTF8();

            int length = control->GetLength();

            CXUnsavedFile file = {buffer.data(), textBuf.data(), length};

            if (!unitCreated)
            {


            const char** args = new const char*[1];
            args[0] = "-I/usr/lib/clang/2.9/include";
            CXIndex index = clang_createIndex(0,0);
            unit = clang_parseTranslationUnit(index, buffer.data(),args,1, &file,1, CXTranslationUnit_PrecompiledPreamble | CXTranslationUnit_CacheCompletionResults | CXTranslationUnit_CXXPrecompiledPreamble);
            unitCreated = true;
            }
            else
            int status = clang_reparseTranslationUnit(unit,1,&file, clang_defaultReparseOptions(unit));





            int line = control->GetCurrentLine() +1;
            int column = control->GetColumn(control->GetCurrentPos()) +2;


            CXCodeCompleteResults* foo= clang_codeCompleteAt(unit,buffer.data(),line,column, &file, 1 , clang_defaultCodeCompleteOptions());

            int pos   = control->GetCurrentPos();
            int start = control->WordStartPosition(pos, true);

            wxArrayString items;



            int a = foo->NumResults;


            for (int i = 0; i < a; i++)
            {
                CXCompletionResult res = foo->Results[i];
                CXCompletionString str = res.CompletionString;
                CXString str2 = clang_getCompletionChunkText(str,1);
                const char* stu = clang_getCString(str2);


                Manager::Get()->GetLogManager()->Log(wxString(stu,wxConvUTF8));
                items.Add(wxString(stu,wxConvUTF8));
            }

            wxString final = GetStringFromArray(items, _(" "));

            //control->CallTipShow(control->GetCurrentPos(), _("This is confusing"));
            control->AutoCompShow(pos-start,final );
           // Manager::Get()->GetLogManager()->Log(result);


            ProjectBuildTarget *target = Manager::Get()->GetProjectManager()->GetActiveProject()->GetBuildTarget(0);
            wxString test = target->GetCompilerID();
            Compiler * comp = CompilerFactory::GetCompiler(test);

            wxArrayString next = comp->GetCompilerSearchDirs(target);

            wxString pray = GetStringFromArray(next, _(" "));

            Manager::Get()->GetLogManager()->Log(pray);

        }
        lastKey = ch;
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

    unitCreated = false;



}

void ClangComplete::OnRelease(bool appShutDown)
{
    // do de-initialization for your plugin
    // if appShutDown is true, the plugin is unloaded because Code::Blocks is being shut down,
    // which means you must not use any of the SDK Managers
    // NOTE: after this function, the inherited member variable
    // m_IsAttached will be FALSE...
    EditorHooks::UnregisterHook(hookId, true);
}
