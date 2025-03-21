/////////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/dirdlg.cpp
// Purpose:     native implementation of wxDirDialog
// Author:      Robert Roebling, Zbigniew Zagorski, Mart Raudsepp, Francesco Montorsi
// Copyright:   (c) 1998 Robert Roebling, 2004 Zbigniew Zagorski, 2005 Mart Raudsepp
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"



/*
  NOTE: the GtkFileChooser interface can be used both for wxFileDialog and for wxDirDialog.
        Thus following code is very similar (even if not identic) to src/gtk/filedlg.cpp
        If you find a problem in this code, remember to check also that file !
*/



#if wxUSE_DIRDLG

#include "wx/dirdlg.h"

#ifndef WX_PRECOMP
    #include "wx/intl.h"
    #include "wx/filedlg.h"
#endif

#include "wx/modalhook.h"
#include "wx/gtk/private.h"
#include "wx/gtk/private/mnemonics.h"
#include "wx/gtk/private/gtk3-compat.h"
#include "wx/stockitem.h"

extern "C" {
static void gtk_dirdialog_response_callback(GtkWidget * WXUNUSED(w),
                                             gint response,
                                             wxDirDialog *dialog)
{
    if (response == GTK_RESPONSE_ACCEPT)
        dialog->GTKOnAccept();
    else // GTK_RESPONSE_CANCEL or GTK_RESPONSE_NONE
        dialog->GTKOnCancel();
}
}

//-----------------------------------------------------------------------------
// wxDirDialog
//-----------------------------------------------------------------------------

wxIMPLEMENT_DYNAMIC_CLASS(wxDirDialog, wxDialog);

wxDirDialog::wxDirDialog(wxWindow* parent,
                         const wxString& title,
                         const wxString& defaultPath,
                         long style,
                         const wxPoint& pos,
                         const wxSize& WXUNUSED(sz),
                         const wxString& WXUNUSED(name))
{
    Create(parent, title, defaultPath, style, pos);
}

bool wxDirDialog::Create(wxWindow* parent,
                         const wxString& title,
                         const wxString& defaultPath,
                         long style,
                         const wxPoint& pos,
                         const wxSize& WXUNUSED(sz),
                         const wxString& WXUNUSED(name))
{
    m_message = title;

    wxASSERT_MSG( !( (style & wxDD_MULTIPLE) && (style & wxDD_CHANGE_DIR) ),
                  "wxDD_CHANGE_DIR can't be used together with wxDD_MULTIPLE" );

    parent = GetParentForModalDialog(parent, style);

    if (!PreCreation(parent, pos, wxDefaultSize) ||
        !CreateBase(parent, wxID_ANY, pos, wxDefaultSize, style,
                wxDefaultValidator, wxT("dirdialog")))
    {
        wxFAIL_MSG( wxT("wxDirDialog creation failed") );
        return false;
    }

    GtkWindow* gtk_parent = nullptr;
    if (parent)
        gtk_parent = GTK_WINDOW( gtk_widget_get_toplevel(parent->m_widget) );

    m_widget = gtk_file_chooser_dialog_new(
                   m_message.utf8_str(),
                   gtk_parent,
                   GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
#ifdef __WXGTK4__
                   static_cast<const char*>(wxConvertMnemonicsToGTK(wxGetStockLabel(wxID_CANCEL)).utf8_str()),
#else
                   "gtk-cancel",
#endif
                   GTK_RESPONSE_CANCEL,
#ifdef __WXGTK4__
                   static_cast<const char*>(wxConvertMnemonicsToGTK(wxGetStockLabel(wxID_OPEN)).utf8_str()),
#else
                   "gtk-open",
#endif
                   GTK_RESPONSE_ACCEPT,
                   nullptr);

    g_object_ref(m_widget);

#if GTK_CHECK_VERSION(3,20,0)
    if (wx_is_at_least_gtk3(20))
    {
        m_fileChooser = GTK_FILE_CHOOSER(gtk_file_chooser_native_new(
            m_message.utf8_str(),
            gtk_parent,
            GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
            nullptr, nullptr));
    }
    else
#endif
    {
        m_fileChooser = GTK_FILE_CHOOSER(m_widget);
        g_object_ref(m_fileChooser);
    }

    gtk_dialog_set_default_response(GTK_DIALOG(m_widget), GTK_RESPONSE_ACCEPT);
#if GTK_CHECK_VERSION(2,18,0)
    if (wx_is_at_least_gtk2(18))
    {
        gtk_file_chooser_set_create_folders(
            m_fileChooser, !HasFlag(wxDD_DIR_MUST_EXIST));
    }
#endif

    // Enable multiple selection if desired
    gtk_file_chooser_set_select_multiple(m_fileChooser, HasFlag(wxDD_MULTIPLE));

    // Enable show hidden folders if desired
    gtk_file_chooser_set_show_hidden(m_fileChooser, HasFlag(wxDD_SHOW_HIDDEN));

    // local-only property could be set to false to allow non-local files to be loaded.
    // In that case get/set_uri(s) should be used instead of get/set_filename(s) everywhere
    // and the GtkFileChooserDialog should probably also be created with a backend,
    // e.g. "gnome-vfs", "default", ... (gtk_file_chooser_dialog_new_with_backend).
    // Currently local-only is kept as the default - true:
    // gtk_file_chooser_set_local_only(GTK_FILE_CHOOSER(m_widget), true);

    g_signal_connect (m_widget, "response",
        G_CALLBACK (gtk_dirdialog_response_callback), this);

    if ( !defaultPath.empty() )
        SetPath(defaultPath);

    return true;
}

wxDirDialog::~wxDirDialog()
{
    g_object_unref(m_fileChooser);
}

void wxDirDialog::GTKAccept()
{
    GSList *fnamesi = gtk_file_chooser_get_filenames(m_fileChooser);
    GSList *fnames = fnamesi;

    while ( fnamesi )
    {
        wxString dir(wxString::FromUTF8(static_cast<gchar *>(fnamesi->data)));
        m_paths.Add(dir);

        g_free(fnamesi->data);
        fnamesi = fnamesi->next;
    }

    g_slist_free(fnames);

    // change to the directory where the user went if asked
    if (HasFlag(wxDD_CHANGE_DIR))
    {
        wxSetWorkingDirectory(m_paths.Last());
    }

    if (!HasFlag(wxDD_MULTIPLE))
    {
        m_path = m_paths.Last();
    }
}

void wxDirDialog::GTKOnAccept()
{
    GTKAccept();
    EndDialog(wxID_OK);
}

void wxDirDialog::GTKOnCancel()
{
    EndDialog(wxID_CANCEL);
}

int wxDirDialog::ShowModal()
{
    WX_HOOK_MODAL_DIALOG();

#if GTK_CHECK_VERSION(3,20,0)
    if (m_fileChooser != (GtkFileChooser*)m_widget)
    {
        m_returnCode = 0;
        int res = gtk_native_dialog_run(GTK_NATIVE_DIALOG(m_fileChooser));
        if (res == GTK_RESPONSE_ACCEPT)
        {
            GTKAccept();
            m_returnCode = wxID_OK;
        }
        else if (m_returnCode == 0)
            m_returnCode = wxID_CANCEL;

        return m_returnCode;
    }
#endif
    return BaseType::ShowModal();
}

void wxDirDialog::EndModal(int retCode)
{
#if GTK_CHECK_VERSION(3,20,0)
    if (m_fileChooser != (GtkFileChooser*)m_widget)
    {
        m_returnCode = retCode;
        gtk_native_dialog_hide(GTK_NATIVE_DIALOG(m_fileChooser));
    }
    else
#endif
    {
        BaseType::EndModal(retCode);
    }
}

void wxDirDialog::DoSetSize(int x, int y, int width, int height, int sizeFlags)
{
    if (!m_wxwindow)
        return;

    BaseType::DoSetSize(x, y, width, height, sizeFlags);
}

void wxDirDialog::SetPath(const wxString& dir)
{
    if (wxDirExists(dir))
    {
        gtk_file_chooser_set_current_folder(m_fileChooser, wxGTK_CONV_FN(dir));
    }
}

#endif // wxUSE_DIRDLG
