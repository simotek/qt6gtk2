/***************************************************************************
 *   Copyright (C) 2015 The Qt Company Ltd.                                *
 *   Copyright (C) 2016-2023 Ilya Kotov, forkotov02@ya.ru                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.         *
 ***************************************************************************/

#include "qt6gtk2dialoghelpers.h"

#include <qeventloop.h>
#include <qwindow.h>
#include <qcolor.h>
#include <qdebug.h>
#include <qfont.h>
#include <qfileinfo.h>

#include <private/qguiapplication_p.h>
#include <qpa/qplatformfontdatabase.h>

#undef signals
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <pango/pango.h>

// The size of the preview we display for selected image files. We set height
// larger than width because generally there is more free space vertically
// than horiztonally (setting the preview image will alway expand the width of
// the dialog, but usually not the height). The image's aspect ratio will always
// be preserved.
#define PREVIEW_WIDTH 256
#define PREVIEW_HEIGHT 512

QT_BEGIN_NAMESPACE

class QGtk2Dialog : public QWindow
{
    Q_OBJECT

public:
    QGtk2Dialog(GtkWidget *gtkWidget);
    ~QGtk2Dialog();

    GtkDialog *gtkDialog() const;

    void exec();
    bool show(Qt::WindowFlags flags, Qt::WindowModality modality, QWindow *parent);
    void hide();

Q_SIGNALS:
    void accept();
    void reject();

protected:
    static void onResponse(QGtk2Dialog *dialog, int response);

private slots:
    void onParentWindowDestroyed();

private:
    GtkWidget *gtkWidget;
};

QGtk2Dialog::QGtk2Dialog(GtkWidget *gtkWidget) : gtkWidget(gtkWidget)
{
    g_signal_connect_swapped(G_OBJECT(gtkWidget), "response", G_CALLBACK(onResponse), this);
    g_signal_connect(G_OBJECT(gtkWidget), "delete-event", G_CALLBACK(gtk_widget_hide_on_delete), nullptr);
}

QGtk2Dialog::~QGtk2Dialog()
{
    gtk_clipboard_store(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD));
    gtk_widget_destroy(gtkWidget);
}

GtkDialog *QGtk2Dialog::gtkDialog() const
{
    return GTK_DIALOG(gtkWidget);
}

void QGtk2Dialog::exec()
{
    if (modality() == Qt::ApplicationModal) {
        // block input to the whole app, including other GTK dialogs
        gtk_dialog_run(gtkDialog());
    } else {
        // block input to the window, allow input to other GTK dialogs
        QEventLoop loop;
        connect(this, SIGNAL(accept()), &loop, SLOT(quit()));
        connect(this, SIGNAL(reject()), &loop, SLOT(quit()));
        loop.exec();
    }
}

bool QGtk2Dialog::show(Qt::WindowFlags flags, Qt::WindowModality modality, QWindow *parent)
{
    connect(parent, &QWindow::destroyed, this, &QGtk2Dialog::onParentWindowDestroyed,
            Qt::UniqueConnection);
    setParent(parent);
    setFlags(flags);
    setModality(modality);

    gtk_widget_realize(gtkWidget); // creates X window

    if (parent) {
        XSetTransientForHint(gdk_x11_drawable_get_xdisplay(gtkWidget->window),
                             gdk_x11_drawable_get_xid(gtkWidget->window),
                             parent->winId());
    }

    if (modality != Qt::NonModal) {
        gdk_window_set_modal_hint(gtkWidget->window, true);
        QGuiApplicationPrivate::showModalWindow(this);
    }

    gtk_widget_show(gtkWidget);
    gdk_window_focus(gtkWidget->window, 0);
    return true;
}

void QGtk2Dialog::hide()
{
    QGuiApplicationPrivate::hideModalWindow(this);
    gtk_widget_hide(gtkWidget);
}

void QGtk2Dialog::onResponse(QGtk2Dialog *dialog, int response)
{
    if (response == GTK_RESPONSE_OK)
        emit dialog->accept();
    else
        emit dialog->reject();
}

void QGtk2Dialog::onParentWindowDestroyed()
{
    // The QGtk2*DialogHelper classes own this object. Make sure the parent doesn't delete it.
    setParent(nullptr);
}

Qt6Gtk2ColorDialogHelper::Qt6Gtk2ColorDialogHelper()
{
    d.reset(new QGtk2Dialog(gtk_color_selection_dialog_new("")));
    connect(d.data(), SIGNAL(accept()), this, SLOT(onAccepted()));
    connect(d.data(), SIGNAL(reject()), this, SIGNAL(reject()));

    GtkWidget *gtkColorSelection = gtk_color_selection_dialog_get_color_selection(GTK_COLOR_SELECTION_DIALOG(d->gtkDialog()));
    g_signal_connect_swapped(gtkColorSelection, "color-changed", G_CALLBACK(onColorChanged), this);
}

Qt6Gtk2ColorDialogHelper::~Qt6Gtk2ColorDialogHelper()
{
}

bool Qt6Gtk2ColorDialogHelper::show(Qt::WindowFlags flags, Qt::WindowModality modality, QWindow *parent)
{
    applyOptions();
    return d->show(flags, modality, parent);
}

void Qt6Gtk2ColorDialogHelper::exec()
{
    d->exec();
}

void Qt6Gtk2ColorDialogHelper::hide()
{
    d->hide();
}

void Qt6Gtk2ColorDialogHelper::setCurrentColor(const QColor &color)
{
    GtkDialog *gtkDialog = d->gtkDialog();
    GtkWidget *gtkColorSelection = gtk_color_selection_dialog_get_color_selection(GTK_COLOR_SELECTION_DIALOG(gtkDialog));
    GdkColor gdkColor;
    gdkColor.red = color.red() << 8;
    gdkColor.green = color.green() << 8;
    gdkColor.blue = color.blue() << 8;
    gtk_color_selection_set_current_color(GTK_COLOR_SELECTION(gtkColorSelection), &gdkColor);
    if (color.alpha() < 255) {
        gtk_color_selection_set_has_opacity_control(GTK_COLOR_SELECTION(gtkColorSelection), true);
        gtk_color_selection_set_current_alpha(GTK_COLOR_SELECTION(gtkColorSelection), color.alpha() << 8);
    }
}

QColor Qt6Gtk2ColorDialogHelper::currentColor() const
{
    GtkDialog *gtkDialog = d->gtkDialog();
    GtkWidget *gtkColorSelection = gtk_color_selection_dialog_get_color_selection(GTK_COLOR_SELECTION_DIALOG(gtkDialog));
    GdkColor gdkColor;
    gtk_color_selection_get_current_color(GTK_COLOR_SELECTION(gtkColorSelection), &gdkColor);
    guint16 alpha = gtk_color_selection_get_current_alpha(GTK_COLOR_SELECTION(gtkColorSelection));
    return QColor(gdkColor.red >> 8, gdkColor.green >> 8, gdkColor.blue >> 8, alpha >> 8);
}

void Qt6Gtk2ColorDialogHelper::onAccepted()
{
    emit accept();
    emit colorSelected(currentColor());
}

void Qt6Gtk2ColorDialogHelper::onColorChanged(Qt6Gtk2ColorDialogHelper *dialog)
{
    emit dialog->currentColorChanged(dialog->currentColor());
}

void Qt6Gtk2ColorDialogHelper::applyOptions()
{
    GtkDialog *gtkDialog = d->gtkDialog();
    gtk_window_set_title(GTK_WINDOW(gtkDialog), options()->windowTitle().toUtf8().constData());

    GtkWidget *gtkColorSelection = gtk_color_selection_dialog_get_color_selection(GTK_COLOR_SELECTION_DIALOG(gtkDialog));
    gtk_color_selection_set_has_opacity_control(GTK_COLOR_SELECTION(gtkColorSelection), options()->testOption(QColorDialogOptions::ShowAlphaChannel));

    GtkWidget *okButton = nullptr;
    GtkWidget *cancelButton = nullptr;
    GtkWidget *helpButton = nullptr;
    g_object_get(G_OBJECT(gtkDialog), "ok-button", &okButton, "cancel-button", &cancelButton, "help-button", &helpButton, nullptr);
    if (okButton)
        g_object_set(G_OBJECT(okButton), "visible", !options()->testOption(QColorDialogOptions::NoButtons), nullptr);
    if (cancelButton)
        g_object_set(G_OBJECT(cancelButton), "visible", !options()->testOption(QColorDialogOptions::NoButtons), nullptr);
    if (helpButton)
        gtk_widget_hide(helpButton);
}

Qt6Gtk2FileDialogHelper::Qt6Gtk2FileDialogHelper()
{
    d.reset(new QGtk2Dialog(gtk_file_chooser_dialog_new("", nullptr,
                                                        GTK_FILE_CHOOSER_ACTION_OPEN,
                                                        GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                                        GTK_STOCK_OK, GTK_RESPONSE_OK, nullptr)));
    connect(d.data(), SIGNAL(accept()), this, SLOT(onAccepted()));
    connect(d.data(), SIGNAL(reject()), this, SIGNAL(reject()));

    g_signal_connect(GTK_FILE_CHOOSER(d->gtkDialog()), "selection-changed", G_CALLBACK(onSelectionChanged), this);
    g_signal_connect_swapped(GTK_FILE_CHOOSER(d->gtkDialog()), "current-folder-changed", G_CALLBACK(onCurrentFolderChanged), this);

    previewWidget = gtk_image_new();
    g_signal_connect(G_OBJECT(d->gtkDialog()), "update-preview", G_CALLBACK(onUpdatePreview), this);
    gtk_file_chooser_set_preview_widget(GTK_FILE_CHOOSER(d->gtkDialog()), previewWidget);
}

Qt6Gtk2FileDialogHelper::~Qt6Gtk2FileDialogHelper()
{
}

bool Qt6Gtk2FileDialogHelper::show(Qt::WindowFlags flags, Qt::WindowModality modality, QWindow *parent)
{
    _dir.clear();
    _selection.clear();

    applyOptions();
    return d->show(flags, modality, parent);
}

void Qt6Gtk2FileDialogHelper::exec()
{
    d->exec();
}

void Qt6Gtk2FileDialogHelper::hide()
{
    // After GtkFileChooserDialog has been hidden, gtk_file_chooser_get_current_folder()
    // & gtk_file_chooser_get_filenames() will return bogus values -> cache the actual
    // values before hiding the dialog
    _dir = directory();
    _selection = selectedFiles();

    d->hide();
}

bool Qt6Gtk2FileDialogHelper::defaultNameFilterDisables() const
{
    return false;
}

void Qt6Gtk2FileDialogHelper::setDirectory(const QUrl &directory)
{
    GtkDialog *gtkDialog = d->gtkDialog();
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(gtkDialog), directory.toLocalFile().toUtf8().constData());
}

QUrl Qt6Gtk2FileDialogHelper::directory() const
{
    // While GtkFileChooserDialog is hidden, gtk_file_chooser_get_current_folder()
    // returns a bogus value -> return the cached value before hiding
    if (!_dir.isEmpty())
        return _dir;

    QString ret;
    GtkDialog *gtkDialog = d->gtkDialog();
    gchar *folder = gtk_file_chooser_get_current_folder(GTK_FILE_CHOOSER(gtkDialog));
    if (folder) {
        ret = QString::fromUtf8(folder);
        g_free(folder);
    }
    return QUrl::fromLocalFile(ret);
}

void Qt6Gtk2FileDialogHelper::selectFile(const QUrl &filename)
{
    GtkDialog *gtkDialog = d->gtkDialog();
    if (options()->acceptMode() == QFileDialogOptions::AcceptSave) {
        QFileInfo fi(filename.toLocalFile());
        gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(gtkDialog), fi.path().toUtf8().constData());
        gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(gtkDialog), fi.fileName().toUtf8().constData());
    } else {
        gtk_file_chooser_select_filename(GTK_FILE_CHOOSER(gtkDialog), filename.toLocalFile().toUtf8().constData());
    }
}

QList<QUrl> Qt6Gtk2FileDialogHelper::selectedFiles() const
{
    // While GtkFileChooserDialog is hidden, gtk_file_chooser_get_filenames()
    // returns a bogus value -> return the cached value before hiding
    if (!_selection.isEmpty())
        return _selection;

    QList<QUrl> selection;
    GtkDialog *gtkDialog = d->gtkDialog();
    GSList *filenames = gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(gtkDialog));
    for (GSList *it  = filenames; it; it = it->next)
        selection += QUrl::fromLocalFile(QString::fromUtf8((const char*)it->data));
    g_slist_free(filenames);
    return selection;
}

void Qt6Gtk2FileDialogHelper::setFilter()
{
    applyOptions();
}

void Qt6Gtk2FileDialogHelper::selectNameFilter(const QString &filter)
{
    GtkFileFilter *gtkFilter = _filters.value(filter);
    if (gtkFilter) {
        GtkDialog *gtkDialog = d->gtkDialog();
        gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(gtkDialog), gtkFilter);
    }
}

QString Qt6Gtk2FileDialogHelper::selectedNameFilter() const
{
    GtkDialog *gtkDialog = d->gtkDialog();
    GtkFileFilter *gtkFilter = gtk_file_chooser_get_filter(GTK_FILE_CHOOSER(gtkDialog));
    return _filterNames.value(gtkFilter);
}

void Qt6Gtk2FileDialogHelper::onAccepted()
{
    emit accept();

    QString filter = selectedNameFilter();
    if (filter.isEmpty())
        emit filterSelected(filter);

    QList<QUrl> files = selectedFiles();
    emit filesSelected(files);
    if (files.count() == 1)
        emit fileSelected(files.first());
}

void Qt6Gtk2FileDialogHelper::onSelectionChanged(GtkDialog *gtkDialog, Qt6Gtk2FileDialogHelper *helper)
{
    QString selection;
    gchar *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(gtkDialog));
    if (filename) {
        selection = QString::fromUtf8(filename);
        g_free(filename);
    }
    emit helper->currentChanged(QUrl::fromLocalFile(selection));
}

void Qt6Gtk2FileDialogHelper::onCurrentFolderChanged(Qt6Gtk2FileDialogHelper *dialog)
{
    emit dialog->directoryEntered(dialog->directory());
}

void Qt6Gtk2FileDialogHelper::onUpdatePreview(GtkDialog *gtkDialog, Qt6Gtk2FileDialogHelper *helper)
{
    gchar *filename = gtk_file_chooser_get_preview_filename(GTK_FILE_CHOOSER(gtkDialog));
    if (!filename) {
        gtk_file_chooser_set_preview_widget_active(GTK_FILE_CHOOSER(gtkDialog), false);
        return;
    }

    // Don't attempt to open anything which isn't a regular file. If a named pipe,
    // this may hang.
    QFileInfo fileinfo(filename);
    if (!fileinfo.exists() || !fileinfo.isFile()) {
        g_free(filename);
        gtk_file_chooser_set_preview_widget_active(GTK_FILE_CHOOSER(gtkDialog), false);
        return;
    }

    // This will preserve the image's aspect ratio.
    GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file_at_size(filename, PREVIEW_WIDTH, PREVIEW_HEIGHT, 0);
    g_free(filename);
    if (pixbuf) {
        gtk_image_set_from_pixbuf(GTK_IMAGE(helper->previewWidget), pixbuf);
        g_object_unref(pixbuf);
    }
    gtk_file_chooser_set_preview_widget_active(GTK_FILE_CHOOSER(gtkDialog), pixbuf ? true : false);
}

static GtkFileChooserAction gtkFileChooserAction(const QSharedPointer<QFileDialogOptions> &options)
{
    switch (options->fileMode()) {
    case QFileDialogOptions::AnyFile:
    case QFileDialogOptions::ExistingFile:
    case QFileDialogOptions::ExistingFiles:
        if (options->acceptMode() == QFileDialogOptions::AcceptOpen)
            return GTK_FILE_CHOOSER_ACTION_OPEN;
        else
            return GTK_FILE_CHOOSER_ACTION_SAVE;
    case QFileDialogOptions::Directory:
    case QFileDialogOptions::DirectoryOnly:
    default:
        if (options->acceptMode() == QFileDialogOptions::AcceptOpen)
            return GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER;
        else
            return GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER;
    }
}

void Qt6Gtk2FileDialogHelper::applyOptions()
{
    GtkDialog *gtkDialog = d->gtkDialog();
    const QSharedPointer<QFileDialogOptions> &opts = options();

    gtk_window_set_title(GTK_WINDOW(gtkDialog), opts->windowTitle().toUtf8().constData());
    gtk_file_chooser_set_local_only(GTK_FILE_CHOOSER(gtkDialog), true);

    const GtkFileChooserAction action = gtkFileChooserAction(opts);
    gtk_file_chooser_set_action(GTK_FILE_CHOOSER(gtkDialog), action);

    const bool selectMultiple = opts->fileMode() == QFileDialogOptions::ExistingFiles;
    gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(gtkDialog), selectMultiple);

    const bool confirmOverwrite = !opts->testOption(QFileDialogOptions::DontConfirmOverwrite);
    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(gtkDialog), confirmOverwrite);

    const QStringList nameFilters = opts->nameFilters();
    if (!nameFilters.isEmpty())
        setNameFilters(nameFilters);

    if (opts->initialDirectory().isLocalFile())
        setDirectory(opts->initialDirectory());

    for (const QUrl &filename : opts->initiallySelectedFiles())
        selectFile(filename);

    const QString initialNameFilter = opts->initiallySelectedNameFilter();
    if (!initialNameFilter.isEmpty())
        selectNameFilter(initialNameFilter);

#if GTK_CHECK_VERSION(2, 20, 0)
    GtkWidget *acceptButton = gtk_dialog_get_widget_for_response(gtkDialog, GTK_RESPONSE_OK);
    if (acceptButton) {
        if (opts->isLabelExplicitlySet(QFileDialogOptions::Accept))
            gtk_button_set_label(GTK_BUTTON(acceptButton), opts->labelText(QFileDialogOptions::Accept).toUtf8().constData());
        else if (opts->acceptMode() == QFileDialogOptions::AcceptOpen)
            gtk_button_set_label(GTK_BUTTON(acceptButton), GTK_STOCK_OPEN);
        else
            gtk_button_set_label(GTK_BUTTON(acceptButton), GTK_STOCK_SAVE);
    }

    GtkWidget *rejectButton = gtk_dialog_get_widget_for_response(gtkDialog, GTK_RESPONSE_CANCEL);
    if (rejectButton) {
        if (opts->isLabelExplicitlySet(QFileDialogOptions::Reject))
            gtk_button_set_label(GTK_BUTTON(rejectButton), opts->labelText(QFileDialogOptions::Reject).toUtf8().constData());
        else
            gtk_button_set_label(GTK_BUTTON(rejectButton), GTK_STOCK_CANCEL);
    }
#endif
}

void Qt6Gtk2FileDialogHelper::setNameFilters(const QStringList &filters)
{
    GtkDialog *gtkDialog = d->gtkDialog();
    for (GtkFileFilter *filter : qAsConst(_filters))
        gtk_file_chooser_remove_filter(GTK_FILE_CHOOSER(gtkDialog), filter);

    _filters.clear();
    _filterNames.clear();

    for (const QString &filter : qAsConst(filters)) {
        GtkFileFilter *gtkFilter = gtk_file_filter_new();
        const QString name = filter.left(filter.indexOf(QLatin1Char('(')));
        const QStringList extensions = cleanFilterList(filter);

        gtk_file_filter_set_name(gtkFilter, name.isEmpty() ? extensions.join(QStringLiteral(", ")).toUtf8().constData() :
                                                             name.toUtf8().constData());
        for (const QString &ext : qAsConst(extensions))
            gtk_file_filter_add_pattern(gtkFilter, ext.toUtf8().constData());

        gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(gtkDialog), gtkFilter);

        _filters.insert(filter, gtkFilter);
        _filterNames.insert(gtkFilter, filter);
    }
}

Qt6Gtk2FontDialogHelper::Qt6Gtk2FontDialogHelper()
{
    d.reset(new QGtk2Dialog(gtk_font_selection_dialog_new("")));
    connect(d.data(), SIGNAL(accept()), this, SLOT(onAccepted()));
    connect(d.data(), SIGNAL(reject()), this, SIGNAL(reject()));
}

Qt6Gtk2FontDialogHelper::~Qt6Gtk2FontDialogHelper()
{
}

bool Qt6Gtk2FontDialogHelper::show(Qt::WindowFlags flags, Qt::WindowModality modality, QWindow *parent)
{
    applyOptions();
    return d->show(flags, modality, parent);
}

void Qt6Gtk2FontDialogHelper::exec()
{
    d->exec();
}

void Qt6Gtk2FontDialogHelper::hide()
{
    d->hide();
}

static QString qt_fontToString(const QFont &font)
{
    PangoFontDescription *desc = pango_font_description_new();
    pango_font_description_set_size(desc, (font.pointSizeF() > 0.0 ? font.pointSizeF() : QFontInfo(font).pointSizeF()) * PANGO_SCALE);
    pango_font_description_set_family(desc, QFontInfo(font).family().toUtf8().constData());

    int weight = font.weight();
    if (weight >= QFont::Black)
        pango_font_description_set_weight(desc, PANGO_WEIGHT_HEAVY);
    else if (weight >= QFont::ExtraBold)
        pango_font_description_set_weight(desc, PANGO_WEIGHT_ULTRABOLD);
    else if (weight >= QFont::Bold)
        pango_font_description_set_weight(desc, PANGO_WEIGHT_BOLD);
    else if (weight >= QFont::DemiBold)
        pango_font_description_set_weight(desc, PANGO_WEIGHT_SEMIBOLD);
    else if (weight >= QFont::Medium)
        pango_font_description_set_weight(desc, PANGO_WEIGHT_MEDIUM);
    else if (weight >= QFont::Normal)
        pango_font_description_set_weight(desc, PANGO_WEIGHT_NORMAL);
    else if (weight >= QFont::Light)
        pango_font_description_set_weight(desc, PANGO_WEIGHT_LIGHT);
    else if (weight >= QFont::ExtraLight)
        pango_font_description_set_weight(desc, PANGO_WEIGHT_ULTRALIGHT);
    else
        pango_font_description_set_weight(desc, PANGO_WEIGHT_THIN);

    int style = font.style();
    if (style == QFont::StyleItalic)
        pango_font_description_set_style(desc, PANGO_STYLE_ITALIC);
    else if (style == QFont::StyleOblique)
        pango_font_description_set_style(desc, PANGO_STYLE_OBLIQUE);
    else
        pango_font_description_set_style(desc, PANGO_STYLE_NORMAL);

    char *str = pango_font_description_to_string(desc);
    QString name = QString::fromUtf8(str);
    pango_font_description_free(desc);
    g_free(str);
    return name;
}

static QFont qt_fontFromString(const QString &name)
{
    QFont font;
    PangoFontDescription *desc = pango_font_description_from_string(name.toUtf8().constData());
    font.setPointSizeF(static_cast<float>(pango_font_description_get_size(desc)) / PANGO_SCALE);

    QString family = QString::fromUtf8(pango_font_description_get_family(desc));
    if (!family.isEmpty())
        font.setFamily(family);

    const int weight = pango_font_description_get_weight(desc);
    font.setWeight(QFont::Weight(weight));

    PangoStyle style = pango_font_description_get_style(desc);
    if (style == PANGO_STYLE_ITALIC)
        font.setStyle(QFont::StyleItalic);
    else if (style == PANGO_STYLE_OBLIQUE)
        font.setStyle(QFont::StyleOblique);
    else
        font.setStyle(QFont::StyleNormal);

    pango_font_description_free(desc);
    return font;
}

void Qt6Gtk2FontDialogHelper::setCurrentFont(const QFont &font)
{
    GtkFontSelectionDialog *gtkDialog = GTK_FONT_SELECTION_DIALOG(d->gtkDialog());
    gtk_font_selection_dialog_set_font_name(gtkDialog, qt_fontToString(font).toUtf8().constData());
}

QFont Qt6Gtk2FontDialogHelper::currentFont() const
{
    GtkFontSelectionDialog *gtkDialog = GTK_FONT_SELECTION_DIALOG(d->gtkDialog());
    gchar *name = gtk_font_selection_dialog_get_font_name(gtkDialog);
    QFont font = qt_fontFromString(QString::fromUtf8(name));
    g_free(name);
    return font;
}

void Qt6Gtk2FontDialogHelper::onAccepted()
{
    emit currentFontChanged(currentFont());
    emit accept();
    emit fontSelected(currentFont());
}

void Qt6Gtk2FontDialogHelper::applyOptions()
{
    GtkDialog *gtkDialog = d->gtkDialog();
    const QSharedPointer<QFontDialogOptions> &opts = options();

    gtk_window_set_title(GTK_WINDOW(gtkDialog), opts->windowTitle().toUtf8().constData());

    GtkWidget *okButton = gtk_font_selection_dialog_get_ok_button(GTK_FONT_SELECTION_DIALOG(gtkDialog));
    GtkWidget *cancelButton = gtk_font_selection_dialog_get_cancel_button(GTK_FONT_SELECTION_DIALOG(gtkDialog));
    if (okButton)
        gtk_widget_set_visible(okButton, !options()->testOption(QFontDialogOptions::NoButtons));
    if (cancelButton)
        gtk_widget_set_visible(cancelButton, !options()->testOption(QFontDialogOptions::NoButtons));
}

QT_END_NAMESPACE

#include "qt6gtk2dialoghelpers.moc"
