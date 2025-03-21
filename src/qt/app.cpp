/////////////////////////////////////////////////////////////////////////////
// Name:        src/qt/app.cpp
// Author:      Peter Most, Javier Torres, Mariano Reingart
// Copyright:   (c) 2010 wxWidgets dev team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#include "wx/app.h"
#include "wx/apptrait.h"
#include "wx/qt/private/utils.h"
#include "wx/qt/private/converter.h"
#include <QtCore/QStringList>
#include <QtWidgets/QApplication>
#include <QSurfaceFormat>

wxIMPLEMENT_DYNAMIC_CLASS(wxApp, wxEvtHandler);

wxApp::wxApp()
{
    m_qtArgc = 0;

    WXAppConstructed();
}


wxApp::~wxApp()
{
    // Delete command line arguments
    for ( int i = 0; i < m_qtArgc; ++i )
    {
        free(m_qtArgv[i]);
    }
}

bool wxApp::Initialize( int& argc_, wxChar** argv_ )
{
    if ( !wxAppBase::Initialize( argc_, argv_ ))
        return false;

    wxConvCurrent = &wxConvUTF8;

    // (See: http://bugreports.qt.nokia.com/browse/QTBUG-7551)
    // Need to store argc, argv. The argc, argv from wxAppBase are
    // being initialized *after* Initialize();

    // TODO: Check whether new/strdup etc. can be replaced with std::vector<>.

    // Clone and store arguments
    m_qtArgv.reset(new char* [argc_ + 1]);
    for ( int i = 0; i < argc_; i++ )
    {
        m_qtArgv[i] = wxStrdupA(wxConvUTF8.cWX2MB(argv_[i]));
    }
    m_qtArgv[argc_] = nullptr;
    m_qtArgc = argc_;

    // Use SingleBuffer mode by default to reduce latency.
    QSurfaceFormat format;
    format.setSwapBehavior(QSurfaceFormat::SwapBehavior::SingleBuffer);
    QSurfaceFormat::setDefaultFormat(format);

    m_qtApplication.reset(new QApplication(m_qtArgc, m_qtArgv.get()));

    if ( m_qtApplication->platformName() == "xcb" )
        m_qtApplication->processEvents(); // Avoids SIGPIPE on X11 when debugging

    // Use the args returned by Qt as it may have deleted (processed) some of them
    // Using QApplication::arguments() forces argument processing
    QStringList qtArgs = m_qtApplication->arguments();
    if ( qtArgs.size() != argc_ )
    {
        /* As per Qt 4.6: Here, qtArgc and qtArgv have been modified and can
         * be used to replace our args (with Qt-flags removed). Also, they can be
         * deleted as they are internally kept by Qt in a list after calling arguments().
         * However, there isn't any guarantee of that in the docs, so we keep arguments
         * ourselves and only delete then after the QApplication is deleted */

        // Qt changed the arguments
        delete [] argv_;
        argv_ = new wxChar *[qtArgs.size() + 1];
        for ( int i = 0; i < qtArgs.size(); i++ )
        {
            argv_[i] = wxStrdupW( wxConvUTF8.cMB2WX( qtArgs[i].toUtf8().data() ) );
        }

        argc_ = m_qtApplication->arguments().size();
        argv_[argc_] = nullptr;
    }

    return true;
}
