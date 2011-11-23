#include "myThread.h"
#include <wx/thread.h>

#include <cbplugin.h>
DEFINE_EVENT_TYPE(wxEVT_MY_EVENT)


void freeCommandLine(const char** args, int numOfTokens)
{

    for (int i = 0; i < numOfTokens; i++)
    {

        free((char*)args[i]);
    }
    free(args);


}

CXTranslationUnit myThread::threadFunc()
{
    CXTranslationUnit unit = clang_parseTranslationUnit(index, buffer.data(),args,numOfTokens, NULL,0, CXTranslationUnit_PrecompiledPreamble | CXTranslationUnit_CacheCompletionResults | CXTranslationUnit_CXXPrecompiledPreamble);
    int status = clang_reparseTranslationUnit(unit,0, NULL, clang_defaultReparseOptions(unit));
    CXCodeCompleteResults* results= clang_codeCompleteAt(unit,buffer.data(),1,1, NULL, 0, clang_defaultCodeCompleteOptions());
    clang_disposeCodeCompleteResults(results);

    freeCommandLine(args,numOfTokens);

    return unit;
}

void* myThread::Entry()
{
    transferData* data = new transferData;
    data->unit = threadFunc();
    data->filename = buffer;


    wxCommandEvent event(wxEVT_MY_EVENT, threadDoneId);
    event.SetClientData(data);
    wxPostEvent(handle,event);
    return 0;

}
