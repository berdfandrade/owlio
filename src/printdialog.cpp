#include "printdialog.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPageLayout>
#include <QPainter>
#include <QPdfWriter>
#include <QPrintDialog>
#include <QPrinter>
#include <QPrinterInfo>
#include <QPushButton>
#include <QScrollBar>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>

#include "theme.h"

namespace {
const QPageLayout kLetter(QPageSize(QPageSize::Letter), QPageLayout::Portrait, QMarginsF(0, 0, 0, 0));
}

// ------------------------------------------------------------ prévia das folhas

SheetPreview::SheetPreview(const Editor& ed, QWidget* parent)
    : QAbstractScrollArea(parent), ed_(ed), painter_(ed) {
    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setMinimumWidth(420);
}

void SheetPreview::setOptions(const PrintOptions& opt) {
    opt_ = opt;
    relayout();
    viewport()->update();
}

std::vector<int> SheetPreview::sheets() const {
    std::vector<int> out;
    if (opt_.titlePage && painter_.hasTitlePage()) out.push_back(-1);
    const int pages = ed_.layout().pageCount;
    const int from = std::clamp(opt_.fromPage, 1, pages), to = std::clamp(opt_.toPage, from, pages);
    for (int i = from - 1; i < to; ++i) out.push_back(i);
    return out;
}

void SheetPreview::relayout() {
    // Uma folha embaixo da outra, como o papel vai sair da impressora.
    const double margin = 32, gap = 28, labelH = 26;
    const double avail = viewport()->width() - 2 * margin;
    cols_ = 1;
    sheetW_ = std::clamp(avail, 160.0, 520.0);
    sheetH_ = sheetW_ * 11.0 / 8.5;
    const int rows = static_cast<int>(sheets().size());
    const double total = margin + rows * (sheetH_ + labelH + gap) - gap + margin;
    verticalScrollBar()->setRange(0, std::max(0, static_cast<int>(total - viewport()->height())));
    verticalScrollBar()->setPageStep(viewport()->height());
    verticalScrollBar()->setSingleStep(48);
}

void SheetPreview::resizeEvent(QResizeEvent* e) {
    QAbstractScrollArea::resizeEvent(e);
    relayout();
}

void SheetPreview::paintEvent(QPaintEvent*) {
    const ThemeColors& tc = ThemeManager::instance().colors();
    QPainter p(viewport());
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    p.fillRect(viewport()->rect(), tc.canvas);

    const auto list = sheets();
    const double margin = 32, gap = 28, labelH = 26;
    const double gridW = cols_ * sheetW_ + (cols_ - 1) * gap;
    const double left = (viewport()->width() - gridW) / 2;
    const double ppi = sheetW_ / 8.5;
    PagePainter::Extras ex;
    ex.sceneNumbers = opt_.sceneNumbers;

    QFont small = font();
    small.setPointSizeF(font().pointSizeF() * 0.86);
    for (int k = 0; k < static_cast<int>(list.size()); ++k) {
        const int row = k / cols_, col = k % cols_;
        const QRectF r(left + col * (sheetW_ + gap),
                       margin + row * (sheetH_ + labelH + gap) - verticalScrollBar()->value(), sheetW_, sheetH_);
        if (r.bottom() + labelH < 0 || r.top() > viewport()->height()) continue;
        p.setPen(Qt::NoPen);
        for (int i = 7; i >= 1; --i) {
            p.setBrush(QColor(0, 0, 0, static_cast<int>((tc.dark ? 0.12 : 0.05) * (1.0 - i / 8.0) * 255)));
            p.drawRoundedRect(r.adjusted(-i, -i + 3, i, i + 3), 2 + i, 2 + i);
        }
        p.setBrush(Qt::white);
        p.drawRect(r);
        p.save();
        p.setClipRect(r);
        if (list[k] < 0) painter_.paintTitlePage(p, r.topLeft(), ppi);
        else painter_.paintPage(p, list[k], r.topLeft(), ppi, ex);
        PagePainter::drawWatermark(p, r.topLeft(), ppi, opt_.watermark);
        p.restore();

        p.setFont(small);
        p.setPen(tc.muted);
        p.drawText(QRectF(r.left(), r.bottom() + 6, r.width(), labelH - 6), Qt::AlignHCenter | Qt::AlignTop,
                   list[k] < 0 ? tr("Title page") : tr("Page %1").arg(list[k] + 1));
    }
    if (list.empty()) {
        p.setPen(tc.muted);
        p.drawText(viewport()->rect(), Qt::AlignCenter, tr("Nothing to print."));
    }
}

// ------------------------------------------------------------ janela

PrintDialog::PrintDialog(const Editor& ed, Target target, const QString& suggestedPdfPath, bool sceneNumbers,
                         QWidget* parent)
    : QDialog(parent), ed_(ed), target_(target), pdfPath_(suggestedPdfPath) {
    setWindowTitle(tr("Print and Export"));
    setMinimumSize(980, 640);
    const int pages = ed.layout().pageCount;
    PagePainter probe(ed);

    preview_ = new SheetPreview(ed);

    // ---- Painel de opções
    heading_ = new QLabel;
    heading_->setObjectName("tituloDialogo");
    summary_ = new QLabel;
    summary_->setObjectName("subtituloDialogo");

    auto& tm = ThemeManager::instance();
    toPrinter_ = new QToolButton;
    toPrinter_->setText(tr("Printer"));
    toPrinter_->setIcon(tm.icon("imprimir"));
    toPdf_ = new QToolButton;
    toPdf_->setText(tr("PDF"));
    toPdf_->setIcon(tm.icon("pdf"));
    auto* targetGroup = new QButtonGroup(this);
    auto* targetRow = new QHBoxLayout;
    targetRow->setSpacing(4);
    for (QToolButton* b : {toPrinter_, toPdf_}) {
        b->setCheckable(true);
        b->setObjectName("segmento");
        b->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        targetGroup->addButton(b);
        targetRow->addWidget(b);
    }
    connect(toPrinter_, &QToolButton::clicked, this, [this] { setTarget(Target::Printer); });
    connect(toPdf_, &QToolButton::clicked, this, [this] { setTarget(Target::Pdf); });

    printer_ = new QComboBox;
    const QStringList printers = QPrinterInfo::availablePrinterNames();
    const QString def = QPrinterInfo::defaultPrinterName();
    for (const QString& n : printers) printer_->addItem(n);
    if (printers.isEmpty()) {
        printer_->addItem(tr("No printers found"));
        printer_->setEnabled(false);
    } else if (!def.isEmpty()) {
        printer_->setCurrentText(def);
    }
    copies_ = new QSpinBox;
    copies_->setRange(1, 99);
    printerBox_ = new QWidget;
    auto* pf = new QFormLayout(printerBox_);
    pf->setContentsMargins(0, 0, 0, 0);
    pf->setVerticalSpacing(10);
    pf->addRow(tr("Printer"), printer_);
    pf->addRow(tr("Copies"), copies_);

    range_ = new QComboBox;
    range_->addItems({tr("All (%n)", "", pages), tr("Range")});
    from_ = new QSpinBox;
    to_ = new QSpinBox;
    for (QSpinBox* sb : {from_, to_}) sb->setRange(1, pages);
    to_->setValue(pages);
    rangeRow_ = new QWidget;
    auto* rr = new QHBoxLayout(rangeRow_);
    rr->setContentsMargins(0, 0, 0, 0);
    rr->addWidget(new QLabel(tr("from")));
    rr->addWidget(from_, 1);
    rr->addWidget(new QLabel(tr("to")));
    rr->addWidget(to_, 1);

    titlePage_ = new QCheckBox(tr("Include title page"));
    titlePage_->setChecked(probe.hasTitlePage());
    titlePage_->setEnabled(probe.hasTitlePage());
    if (!probe.hasTitlePage()) titlePage_->setToolTip(tr("The script has no title yet (Document → Title Page)."));
    sceneNumbers_ = new QCheckBox(tr("Number scenes"));
    sceneNumbers_->setChecked(sceneNumbers);
    watermark_ = new QLineEdit;
    watermark_->setPlaceholderText(tr("E.g. DRAFT, or the name of the recipient"));
    watermark_->setClearButtonEnabled(true);

    auto* contentForm = new QFormLayout;
    contentForm->setVerticalSpacing(10);
    contentForm->addRow(tr("Pages"), range_);
    contentForm->addRow(QString(), rangeRow_);
    contentForm->addRow(tr("Watermark"), watermark_);

    auto section = [](const QString& t) {
        auto* l = new QLabel(t.toUpper());
        l->setObjectName("secao");
        return l;
    };

    go_ = new QPushButton;
    go_->setObjectName("primarioBotao");
    auto* cancel = new QPushButton(tr("Cancel"));
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(go_, &QPushButton::clicked, this, [this] { run(false); });
    advanced_ = new QPushButton(tr("Printer Options…"));
    advanced_->setObjectName("link");
    advanced_->setFlat(true);
    advanced_->setCursor(Qt::PointingHandCursor);
    connect(advanced_, &QPushButton::clicked, this, [this] { run(true); });

    auto* side = new QWidget;
    side->setObjectName("painelLateral");
    side->setFixedWidth(340);
    auto* sl = new QVBoxLayout(side);
    sl->setContentsMargins(24, 24, 24, 20);
    sl->setSpacing(10);
    sl->addWidget(heading_);
    sl->addWidget(summary_);
    sl->addSpacing(12);
    sl->addWidget(section(tr("Destination")));
    sl->addLayout(targetRow);
    sl->addWidget(printerBox_);
    sl->addSpacing(10);
    sl->addWidget(section(tr("Content")));
    sl->addLayout(contentForm);
    sl->addWidget(titlePage_);
    sl->addWidget(sceneNumbers_);
    sl->addStretch(1);
    sl->addWidget(advanced_, 0, Qt::AlignLeft);
    auto* btns = new QHBoxLayout;
    btns->addWidget(cancel);
    btns->addWidget(go_, 1);
    sl->addLayout(btns);

    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addWidget(preview_, 1);
    root->addWidget(side);

    connect(range_, &QComboBox::currentIndexChanged, this, &PrintDialog::refresh);
    connect(from_, &QSpinBox::valueChanged, this, [this](int v) {
        if (to_->value() < v) to_->setValue(v);
        refresh();
    });
    connect(to_, &QSpinBox::valueChanged, this, [this](int v) {
        if (from_->value() > v) from_->setValue(v);
        refresh();
    });
    connect(copies_, &QSpinBox::valueChanged, this, &PrintDialog::refresh);
    for (QCheckBox* c : {titlePage_, sceneNumbers_}) connect(c, &QCheckBox::toggled, this, &PrintDialog::refresh);
    connect(watermark_, &QLineEdit::textChanged, this, &PrintDialog::refresh);

    setTarget(target);
    go_->setDefault(true);  // depois de estar na janela, senão o QDialog escolhe outro
}

PrintOptions PrintDialog::options() const {
    PrintOptions o;
    o.titlePage = titlePage_->isChecked();
    o.sceneNumbers = sceneNumbers_->isChecked();
    if (range_->currentIndex() == 1) {
        o.fromPage = from_->value();
        o.toPage = to_->value();
    }
    o.watermark = watermark_->text().trimmed();
    return o;
}

void PrintDialog::setTarget(Target t) {
    target_ = t;
    toPrinter_->setChecked(t == Target::Printer);
    toPdf_->setChecked(t == Target::Pdf);
    printerBox_->setVisible(t == Target::Printer);
    advanced_->setVisible(t == Target::Printer);
    refresh();
}

void PrintDialog::refresh() {
    rangeRow_->setVisible(range_->currentIndex() == 1);
    const PrintOptions o = options();
    preview_->setOptions(o);

    const int pages = ed_.layout().pageCount;
    const int from = std::clamp(o.fromPage, 1, pages), to = std::clamp(o.toPage, from, pages);
    const int sheets = (to - from + 1) + (o.titlePage && titlePage_->isEnabled() ? 1 : 0);
    const bool printer = target_ == Target::Printer;
    heading_->setText(printer ? tr("Print") : tr("Export PDF"));
    QString info = tr("%n sheet(s)", "", sheets) + "  ·  " + tr("US Letter");
    if (printer && copies_->value() > 1) info += "  ·  " + tr("%n copies", "", copies_->value());
    summary_->setText(info);
    go_->setText(printer ? tr("Print") : tr("Save PDF…"));
    go_->setEnabled(!printer || printer_->isEnabled());
}

void PrintDialog::run(bool advanced) {
    const PrintOptions o = options();
    if (target_ == Target::Pdf) {
        QString path = QFileDialog::getSaveFileName(this, tr("Export PDF"), pdfPath_, tr("PDF (*.pdf)"));
        if (path.isEmpty()) return;
        if (!path.endsWith(".pdf", Qt::CaseInsensitive)) path += ".pdf";
        QPdfWriter w(path);
        w.setResolution(1200);
        w.setPageLayout(kLetter);
        w.setTitle(QString::fromStdString(ed_.doc().titleValue("Title")));
        w.setCreator("Owlio");
        renderScript(w, w.resolution(), ed_, o);
        if (QFileInfo(path).size() <= 0) {
            QMessageBox::warning(this, tr("Export PDF"), tr("Could not write %1.").arg(path));
            return;
        }
        exported_ = path;
        accept();
        return;
    }

    QPrinter printer(QPrinter::HighResolution);
    printer.setPrinterName(printer_->currentText());
    printer.setCopyCount(copies_->value());
    printer.setFullPage(true);
    printer.setPageLayout(kLetter);
    printer.setDocName(QString::fromStdString(ed_.doc().titleValue("Title")));
    if (advanced) {
        QPrintDialog dlg(&printer, this);
        dlg.setOption(QAbstractPrintDialog::PrintPageRange, false);
        if (dlg.exec() != QDialog::Accepted) return;
    }
    renderScript(printer, printer.resolution(), ed_, o);
    accept();
}
