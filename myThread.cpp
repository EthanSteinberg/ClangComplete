#include "myThread.h"
#include <wx/thread.h>

#include <cbplugin.h>
DEFINE_EVENT_TYPE(wxEVT_MY_EVENT)

int threadDoneId = wxNewId();

void freeCommandLine(const char** args, int numOfTokens)
{

    for (int i = 0; i < numOfTokens; i++)
    {

        free((char*)args[i+1]);
    }
    free(args);


}

CXTranslationUnit myThread::threadFunc()
{
    CXUnsavedFile file = {buffer.data(), textBuf.data(), length};
    CXTranslationUnit unit = clang_parseTranslationUnit(index, buffer.data(),args,numOfTokens+1, &file,1, CXTranslationUnit_PrecompiledPreamble | CXTranslationUnit_CacheCompletionResults | CXTranslationUnit_CXXPrecompiledPreamble);
    int status = clang_reparseTranslationUnit(unit,1,&file, clang_defaultReparseOptions(unit));
    CXCodeCompleteResults* results= clang_codeCompleteAt(unit,buffer.data(),1,1, &file, 1 , clang_defaultCodeCompleteOptions());
    clang_disposeCodeCompleteResults(results);

    freeCommandLine(args,numOfTokens);

    return unit;
}

void* myThread::Entry()
{



    wxCommandEvent event(wxEVT_MY_EVENT, threadDoneId);
    event.SetClientData(threadFunc());
    wxPostEvent(handle,event);
    return 0;

}
