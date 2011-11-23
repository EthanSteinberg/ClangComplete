#ifndef MYTHREAD_H_INCLUDED
#define MYTHREAD_H_INCLUDED

#include <wx/wxprec.h>

#ifndef WX_PRECOMP
    #include <wx/wx.h>
#endif

#include <clang-c/Index.h>


DECLARE_EVENT_TYPE(wxEVT_MY_EVENT, -1)

class cbPlugin;


extern int threadDoneId;


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

    CXTranslationUnit threadFunc();


    virtual ExitCode Entry();

};


#endif
