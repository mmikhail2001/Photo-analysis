#include <QPushButton>
#include <QString>
#include <QVector>
#include <QMessageBox>
#include <QFileDialog>
#include <QTabWidget>
#include <QDebug>
#include <regex>

#include "stdafx.h"

#include "win.h"
#include "ui_win.h"

std::vector<ReportExtraction> AnalyzeDirectoryImages(std::filesystem::path const &directory_path, ExtracterExif &extracterExif);

Win::Win(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Win)
{
    ui->setupUi(this); 

    QObject::connect(ui->buttonExit,    &QPushButton::clicked,  this,   &QWidget::close);
    QObject::connect(ui->buttonAnalyze, &QPushButton::clicked,  this,   &Win::showFormAnalyze);
    QObject::connect(ui->buttonOpenDir, &QPushButton::clicked,  this,   &Win::showFileBrowser);
    QObject::connect(ui->tabWidget, &QTabWidget::tabCloseRequested, this, &Win::closeTab);

    prepareWindow();
}

void Win::showFileBrowser()
{
    QString filename = QFileDialog::getExistingDirectory(
        this, 
        "Open directory",
        "/home/mikhail",
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
    );

    ui->lineEdit_OpenDir->setText(filename);

    // Фокус на кнопку "Анализировать" 
    ui->buttonAnalyze->setFocus();
    ui->buttonAnalyze->setDefault(true);
}

void Win::closeTab(int index)
{
    if (index != 0)
    {
        ui->tabWidget->removeTab(index);
    }
    else
    {
        QMessageBox::warning(this, "Предупреждение", "Главную вкладку нельзя закрыть");
    }
}


void Win::showFormAnalyze()
{
    // Заполнение векторов тегами

    std::vector<uint16_t> vecTags;
    std::vector<uint16_t> vecRefs;
    
    fillVectorFromCheckBoxes(vecTags, vecRefs);

    if (!fillVectorFromLineEdit(vecTags, vecRefs))
    {
        QMessageBox::warning(this, "Предупреждение", "Список тегов введен некорректно");
        return;
    }
   
    // Инициализация объекта по извлечению метаданных

    extracterExif.AddTags(vecTags);
    extracterExif.AddRefs(vecRefs);

    // Извлечение метаданных фотографий из указанной директории

    std::string path = ui->lineEdit_OpenDir->text().toStdString();
    std::filesystem::path file_directory{path};

    std::vector<ReportExtraction> vecReports = AnalyzeDirectoryImages(file_directory, extracterExif);

    // Подсчет количества строк в таблице

    size_t nRows = getCountRows(vecReports);

    if (nRows == 0)
    {
        QMessageBox::warning(this, "Предупреждение", "Директория не содержит фотографии формата JPEG");
        return;
    }

    // Создание виджета с таблицей извлеченных метаданных

    formAnalyze = new FormAnalyze(nRows, vecTags.size() + 1);

    // Заполнение вектора названий полей метаданных

    // "title" + названия метаданных
    std::vector<std::string> vecHeaders(vecTags.size() + 1);
    
    fillVectorFields(vecTags, vecHeaders);

    formAnalyze->SetHorizontalHeaders(vecHeaders);
    
    // Заполнение таблицы

    fillTableWithReports(vecTags, vecReports);

    // Уводомление с указанием фотографий, в которых не найден формат Exif

    notifyAboutFilesWithoutExif(vecReports);

    formAnalyze->SetOptimalSize();
    // Добавление новой вкладки с таблицей

    showWidgetInTab(formAnalyze);
}

Win::~Win()
{
    delete ui;
}

void Win::prepareWindow()
{
    ui->lineEdit_OpenDir->setPlaceholderText("/home/user/photos");
    ui->lineEdit_Refs->setPlaceholderText("0x8825");
    ui->lineEdit_Tags->setPlaceholderText("0x0001 0x0002 0x0003");

    ui->label_HyperLink->setTextInteractionFlags(Qt::LinksAccessibleByMouse);
    ui->label_HyperLink->setOpenExternalLinks(true);
    ui->label_HyperLink->setText("<a href=\"https://www.media.mit.edu/pia/Research/deepview/exif.html\">Документация на стандарт Exif</a>");

    ui->label_HyperLinkListTags->setTextInteractionFlags(Qt::LinksAccessibleByMouse);
    ui->label_HyperLinkListTags->setOpenExternalLinks(true);
    ui->label_HyperLinkListTags->setText("<a href=\"https://exiftool.org/TagNames/EXIF.html\">Список тегов стандарта Exif</a>");

    QPixmap pix("/home/mikhail/qt_projects/Photo-analysis/static/logo_jpg.png");
    ui->label_ImageLogo->setPixmap(pix);
    ui->label_ImageLogo->setScaledContents(true);
    int pixWidth = pix.width();
    int labelWidth = ui->label_ImageLogo->width();
    double factor = (double)labelWidth / pixWidth;
    ui->label_ImageLogo->setFixedWidth(factor * pix.width());
    ui->label_ImageLogo->setFixedHeight(factor * pix.height());


    ui->buttonOpenDir->setToolTip("Выберите директорию с фотографиями для извлечения метаданных");
    ui->label_Tags->setToolTip("Выберите свойства, которых хотите извлечь из фотографий");
    ui->label_Refs->setToolTip(
        "Если теги, которые вы ввели выше, не расположены в следующих каталогах <br>\
        <strong>IFD0, SubIFD, IFD1</strong>,<br>введите теги, ссылающиеся на директории тегов, для поиска искомых тегов в них.<br><br>\
        <i>Теги в шестнадцатеричном формате, например 0x8891, 0x9345</i>"
    );
    ui->label_OwnTags->setToolTip(
        "Введите интересующие теги<br><br>\
        <i>Теги в шестнадцатеричном формате, например 0x8891, 0x9345</i>"
    );

    ui->tabWidget->removeTab(1);
    ui->tabWidget->setTabText(0, "Главная");
}

void Win::fillVectorFromCheckBoxes(std::vector<uint16_t> &vecTags, std::vector<uint16_t> &vecRefs)
{
    // Дата и время (тег по умолчанию)
    vecTags.push_back(0x9003); 
    
    if (ui->checkBox_ET->isChecked())
    {
        vecTags.push_back(0x829a);
    }
    if (ui->checkBox_ISO->isChecked())
    {
        vecTags.push_back(0x8827);
    }
    if (ui->checkBox_FN->isChecked())
    {
        vecTags.push_back(0x829d);
    }
    if (ui->checkBox_FL->isChecked())
    {
        vecTags.push_back(0x920a);
    }
    if (ui->checkBox_GPS->isChecked())
    {
        vecRefs.push_back(0x8825);
        vecTags.push_back(0x0001);
        vecTags.push_back(0x0002);
        vecTags.push_back(0x0003);
        vecTags.push_back(0x0004);
    }
}

bool Win::fillVectorFromLineEdit(std::vector<uint16_t> &vecTags, std::vector<uint16_t> &vecRefs)
{
    auto parseHexStrings = [](std::string const &tags, std::vector<uint16_t> &vec) -> bool
    {
        std::regex rxVowelsWords("\\b[аеёиоуыэюя]+\\b",
            std::regex_constants::collate | std::regex_constants::icase);
        // проверяем всю строку на соответствие
        std::regex regexFullString(R"(((0x[\dabcdf]{4}\b)\s*(as\s*(\w+)|)\s*(,|$)\s*)*)");
        if (!std::regex_match(tags, regexFullString))
        {
            return false;
        }
        // извлекаем теги
        std::regex regexTags(R"(0x[\dabcdf]{4}\b)");
        std::vector<std::string> matches_tags
        {
            std::sregex_token_iterator{tags.begin(), tags.end(), regexTags, 0},
            std::sregex_token_iterator{}
        };
        if (!tags.empty())
        {
            for (size_t i = 0; i < matches_tags.size(); ++i)
            {
                uint16_t tag = (uint16_t)std::stoi(matches_tags[i], nullptr, 16);
                vec.push_back(tag);
            }  
        }    
        return true;
    };
    if (!parseHexStrings(ui->lineEdit_Tags->text().toStdString(), vecTags))
    {
        return false;
    }

    if (!parseHexStrings(ui->lineEdit_Refs->text().toStdString(), vecRefs))
    {
        return false;
    }
    return true;
}

size_t Win::getCountRows(std::vector<ReportExtraction> const &vecReports)
{
    size_t nRows = 0;
    for (size_t i = 0; i < vecReports.size(); ++i)
    {
        if (vecReports[i].done)
        {
            nRows++;
        }
    }
    return nRows;
}

void Win::fillVectorFields(std::vector<uint16_t> const &vecTags,std::vector<std::string> &vecFields)
{
    vecFields[0] = "Фотография";

    std::string inputStr = ui->lineEdit_Tags->text().toStdString();

    // Извлекаем теги с альясами
    std::regex regexTagsAliases(R"(0x([\dabcdf]{4}\b)\s*as\s*(\w+))");
    std::vector<std::smatch> mathesTagAlieses{
        std::sregex_iterator{inputStr.begin(), inputStr.end(), regexTagsAliases},
        std::sregex_iterator{}
    };
    std::unordered_map<std::string, std::string> mapTagAlias;
    for (size_t i = 0; i < mathesTagAlieses.size(); ++i)
    {
        mapTagAlias[mathesTagAlieses[i].str(1)] = mathesTagAlieses[i].str(2);
    }

    std::stringstream sstream;
    for (size_t i = 0; i < vecTags.size(); ++i)
    {
        // clear очищает флаги
        sstream.str(std::string{});
        sstream << std::setfill('0') << std::setw(4) << std::hex << vecTags[i];
        auto it = mapTagAlias.find(sstream.str());
        // Альяс найден
        if (it != mapTagAlias.end())
        {
            vecFields[i + 1] = it->second;
        }
        else 
        {
            auto it = ExtracterExif::mapTagsName.find(vecTags[i]); 
            // Название тега известно
            if (it != ExtracterExif::mapTagsName.end())
            {
                vecFields[i + 1] = ExtracterExif::mapTagsName.at(vecTags[i]);
            }
            // Название тега неизвестно
            else
            {
                sstream.str(std::string{});
                std::string tagStr;
                sstream << "0x" << std::setfill('0') << std::setw(4) << std::hex << vecTags[i];
                sstream >> tagStr;
                vecFields[i + 1] = tagStr;
            }
        }
        
    }
}
 #include <QVariant>

// используем item->setData, который принимает QVariant
// QVariant будет правильно сортироваться (строки лексикографически, числа арифметически)
void Win::fillTableWithReports(std::vector<uint16_t> const &vecTags, std::vector<ReportExtraction> const &vecReports)
{
    size_t row = 0;
    for (size_t i = 0; i < vecReports.size(); ++i)
    {
        if (vecReports[i].done == true)
        {
            QTableWidgetItem *item = new QTableWidgetItem;
            item->setData(Qt::EditRole, QString::fromStdString(vecReports[i].file_name));
            formAnalyze->SetItem(row, 0, item);

            for (size_t j = 0; j < vecTags.size(); ++j)
            {
                auto it = vecReports[i].mapData.find(vecTags[j]);
                QTableWidgetItem *item = new QTableWidgetItem;
                if (it != vecReports[i].mapData.end())
                {
                    double value;
                    if (value = QString::fromStdString(vecReports[i].mapData.at(vecTags[j])).toDouble())
                    {
                        item->setData(Qt::EditRole, value);
                    }
                    else
                    {
                        item->setData(Qt::EditRole, QString::fromStdString(vecReports[i].mapData.at(vecTags[j])));
                    }
                }
                else
                {
                    // пустая строка, если не нашел 
                    item->setData(Qt::EditRole, "");
                }
                formAnalyze->SetItem(row, j + 1, item);
            }
            ++row;
        }
    }
}

void Win::notifyAboutFilesWithoutExif(std::vector<ReportExtraction> const &vecReports)
{
    std::vector<std::string> vecFilesNoExif;

    for (size_t i = 0; i < vecReports.size(); ++i)
    {
        if (vecReports[i].done == false)
        {   
            vecFilesNoExif.push_back(vecReports[i].file_name);
        }
    }

    if (!vecFilesNoExif.empty())
    {
        std::string listFilesNoExif = 
                        std::accumulate(std::next(vecFilesNoExif.begin()), vecFilesNoExif.end(), 
                        vecFilesNoExif[0], [](std::string init, std::string const &strFile){
                                return init + ", " + strFile;
                            });

        QMessageBox::warning(this, "Предупреждение", 
                    "В следующих фотографиях <strong>" + QString::fromStdString(listFilesNoExif) + "</strong> не найден формат Exif");
    }
}

void Win::showWidgetInTab(QWidget *widget)
{
    ui->tabWidget->addTab(widget, "Таблица #" + QString::number(ui->tabWidget->count()));
    ui->tabWidget->setCurrentIndex(ui->tabWidget->count() - 1);
}

std::vector<ReportExtraction> AnalyzeDirectoryImages(std::filesystem::path const &directory_path, ExtracterExif &extracterExif)
{
    std::vector<ReportExtraction> vecReports;
    for (auto const &file : std::filesystem::directory_iterator(directory_path))
    {
        if (file.path().extension().string() == ".jpg" || file.path().extension().string() == ".jpeg")
        {
            EndianFile endianFile(file.path().string());
            vecReports.push_back(extracterExif.ExtractExif(endianFile));
        }
    }
    return vecReports;
}
