// main.cpp
#include <QApplication>
#include <QMainWindow>
#include <QSplitter>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QGroupBox>
#include <QFormLayout>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>
#include <QComboBox>
#include <QStatusBar>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QToolBar>
#include <QFontMetrics>
#include <QProcess>
#include <QSettings>
#include <QSyntaxHighlighter>
#include <QRegularExpression>
#include <QTextCharFormat>

// Simple syntax highlighter for the code editor
class CodeHighlighter : public QSyntaxHighlighter {
public:
    CodeHighlighter(QTextDocument *parent = nullptr) : QSyntaxHighlighter(parent) {
        HighlightingRule rule;

        // Keywords
        keywordFormat.setForeground(Qt::blue);
        keywordFormat.setFontWeight(QFont::Bold);
        QStringList keywordPatterns = {
            "\\bclass\\b", "\\bconst\\b", "\\benum\\b", "\\bexplicit\\b",
            "\\bfriend\\b", "\\binline\\b", "\\bnamespace\\b", "\\boperator\\b",
            "\\bprivate\\b", "\\bprotected\\b", "\\bpublic\\b", "\\bsignals\\b",
            "\\bslots\\b", "\\bstatic\\b", "\\btemplate\\b", "\\btypedef\\b",
            "\\btypename\\b", "\\bunion\\b", "\\bvirtual\\b", "\\bvolatile\\b",
            "\\bbreak\\b", "\\bcase\\b", "\\bcatch\\b", "\\bcontinue\\b",
            "\\bdefault\\b", "\\bdelete\\b", "\\bdo\\b", "\\belse\\b",
            "\\bfor\\b", "\\bif\\b", "\\bnew\\b", "\\breturn\\b",
            "\\bswitch\\b", "\\bthrow\\b", "\\btry\\b", "\\bwhile\\b",
            "\\bauto\\b", "\\bbool\\b", "\\bchar\\b", "\\bdouble\\b",
            "\\bfloat\\b", "\\bint\\b", "\\blong\\b", "\\bshort\\b",
            "\\bsigned\\b", "\\bstruct\\b", "\\bunsigned\\b", "\\bvoid\\b"
        };

        for (const QString &pattern : keywordPatterns) {
            rule.pattern = QRegularExpression(pattern);
            rule.format = keywordFormat;
            highlightingRules.append(rule);
        }

        // Class names
        classFormat.setForeground(Qt::darkMagenta);
        rule.pattern = QRegularExpression("\\bQ[A-Za-z]+\\b");
        rule.format = classFormat;
        highlightingRules.append(rule);

        // Single line comments
        singleLineCommentFormat.setForeground(Qt::darkGreen);
        rule.pattern = QRegularExpression("//[^\n]*");
        rule.format = singleLineCommentFormat;
        highlightingRules.append(rule);

        // Quotations
        quotationFormat.setForeground(Qt::darkRed);
        rule.pattern = QRegularExpression("\".*\"");
        rule.format = quotationFormat;
        highlightingRules.append(rule);
    }

protected:
    void highlightBlock(const QString &text) override {
        for (const HighlightingRule &rule : highlightingRules) {
            QRegularExpressionMatchIterator matchIterator = rule.pattern.globalMatch(text);
            while (matchIterator.hasNext()) {
                QRegularExpressionMatch match = matchIterator.next();
                setFormat(match.capturedStart(), match.capturedLength(), rule.format);
            }
        }
    }

private:
    struct HighlightingRule {
        QRegularExpression pattern;
        QTextCharFormat format;
    };

    QVector<HighlightingRule> highlightingRules;

    QTextCharFormat keywordFormat;
    QTextCharFormat classFormat;
    QTextCharFormat singleLineCommentFormat;
    QTextCharFormat quotationFormat;
};

// Custom QPlainTextEdit for the code editor
class CodeEditor : public QPlainTextEdit {
public:
    CodeEditor(QWidget *parent = nullptr) : QPlainTextEdit(parent) {
        // Line numbers area width
        lineNumberArea = new LineNumberArea(this);
        
        connect(this, &CodeEditor::blockCountChanged, this, &CodeEditor::updateLineNumberAreaWidth);
        connect(this, &CodeEditor::updateRequest, this, &CodeEditor::updateLineNumberArea);
        
        updateLineNumberAreaWidth(0);

        // Set monospace font
        QFont font("Courier New", 10);
        font.setFixedPitch(true);
        this->setFont(font);

        // Set tab width
        QFontMetrics metrics(font);
        this->setTabStopDistance(4 * metrics.horizontalAdvance(' '));

        // Setup syntax highlighter
        highlighter = new CodeHighlighter(this->document());
    }

    int lineNumberAreaWidth() {
        int digits = 1;
        int max = qMax(1, blockCount());
        while (max >= 10) {
            max /= 10;
            ++digits;
        }

        int space = 3 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
        return space;
    }

protected:
    void resizeEvent(QResizeEvent *event) override {
        QPlainTextEdit::resizeEvent(event);
        
        QRect cr = contentsRect();
        lineNumberArea->setGeometry(QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
    }

private slots:
    void updateLineNumberAreaWidth(int newBlockCount) {
        setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
    }

    void updateLineNumberArea(const QRect &rect, int dy) {
        if (dy)
            lineNumberArea->scroll(0, dy);
        else
            lineNumberArea->update(0, rect.y(), lineNumberArea->width(), rect.height());

        if (rect.contains(viewport()->rect()))
            updateLineNumberAreaWidth(0);
    }

    void lineNumberAreaPaintEvent(QPaintEvent *event) {
        QPainter painter(lineNumberArea);
        painter.fillRect(event->rect(), Qt::lightGray);

        QTextBlock block = firstVisibleBlock();
        int blockNumber = block.blockNumber();
        int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
        int bottom = top + qRound(blockBoundingRect(block).height());

        while (block.isValid() && top <= event->rect().bottom()) {
            if (block.isVisible() && bottom >= event->rect().top()) {
                QString number = QString::number(blockNumber + 1);
                painter.setPen(Qt::black);
                painter.drawText(0, top, lineNumberArea->width(), fontMetrics().height(),
                                 Qt::AlignRight, number);
            }

            block = block.next();
            top = bottom;
            bottom = top + qRound(blockBoundingRect(block).height());
            ++blockNumber;
        }
    }

private:
    // Line number area widget
    class LineNumberArea : public QWidget {
    public:
        LineNumberArea(CodeEditor *editor) : QWidget(editor), codeEditor(editor) {}

        QSize sizeHint() const override {
            return QSize(codeEditor->lineNumberAreaWidth(), 0);
        }

    protected:
        void paintEvent(QPaintEvent *event) override {
            codeEditor->lineNumberAreaPaintEvent(event);
        }

    private:
        CodeEditor *codeEditor;
    };

    LineNumberArea *lineNumberArea;
    CodeHighlighter *highlighter;
};

// Terminal emulator widget
class TerminalWidget : public QWidget {
    Q_OBJECT
public:
    TerminalWidget(QWidget *parent = nullptr) : QWidget(parent) {
        QVBoxLayout *layout = new QVBoxLayout(this);
        
        // Terminal output display
        outputDisplay = new QPlainTextEdit(this);
        outputDisplay->setReadOnly(true);
        QFont font("Courier New", 10);
        font.setFixedPitch(true);
        outputDisplay->setFont(font);
        outputDisplay->setStyleSheet("background-color: #2D2D30; color: #DCDCDC;");
        
        // Command input
        commandInput = new QLineEdit(this);
        commandInput->setStyleSheet("background-color: #2D2D30; color: #DCDCDC;");
        commandInput->setPlaceholderText("Enter command...");
        
        connect(commandInput, &QLineEdit::returnPressed, this, &TerminalWidget::executeCommand);
        
        layout->addWidget(outputDisplay);
        layout->addWidget(commandInput);
        
        // Set up process
        process = new QProcess(this);
        connect(process, &QProcess::readyReadStandardOutput, this, &TerminalWidget::readOutput);
        connect(process, &QProcess::readyReadStandardError, this, &TerminalWidget::readError);
        
        // Welcome message
        outputDisplay->appendPlainText("Terminal Ready - Enter commands below");
        outputDisplay->appendPlainText("-------------------------------------");
    }

private slots:
    void executeCommand() {
        QString command = commandInput->text();
        commandInput->clear();
        
        outputDisplay->appendPlainText("> " + command);
        
#ifdef Q_OS_WIN
        process->start("cmd.exe", QStringList() << "/c" << command);
#else
        process->start("/bin/sh", QStringList() << "-c" << command);
#endif
    }
    
    void readOutput() {
        QByteArray output = process->readAllStandardOutput();
        outputDisplay->appendPlainText(QString::fromLocal8Bit(output));
    }
    
    void readError() {
        QByteArray error = process->readAllStandardError();
        outputDisplay->appendHtml("<span style='color: #FF6B68;'>" + 
                                  QString::fromLocal8Bit(error).toHtmlEscaped() + "</span>");
    }

private:
    QPlainTextEdit *outputDisplay;
    QLineEdit *commandInput;
    QProcess *process;
};

// Gemini API client widget
class GeminiWidget : public QWidget {
    Q_OBJECT
public:
    GeminiWidget(QWidget *parent = nullptr) : QWidget(parent) {
        QVBoxLayout *layout = new QVBoxLayout(this);
        
        // API key configuration
        QGroupBox *configGroup = new QGroupBox("Gemini API Configuration", this);
        QFormLayout *configLayout = new QFormLayout(configGroup);
        
        apiKeyInput = new QLineEdit(this);
        apiKeyInput->setEchoMode(QLineEdit::Password);
        apiKeyInput->setPlaceholderText("Enter your Gemini API key");
        configLayout->addRow("API Key:", apiKeyInput);
        
        modelSelector = new QComboBox(this);
        modelSelector->addItems({"gemini-pro", "gemini-ultra"});
        configLayout->addRow("Model:", modelSelector);
        
        saveConfigButton = new QPushButton("Save Configuration", this);
        connect(saveConfigButton, &QPushButton::clicked, this, &GeminiWidget::saveConfig);
        configLayout->addRow("", saveConfigButton);
        
        // Chat interface
        QGroupBox *chatGroup = new QGroupBox("Gemini Chat", this);
        QVBoxLayout *chatLayout = new QVBoxLayout(chatGroup);
        
        chatOutput = new QTextEdit(this);
        chatOutput->setReadOnly(true);
        chatLayout->addWidget(chatOutput);
        
        chatInput = new QTextEdit(this);
        chatInput->setPlaceholderText("Type your message to Gemini...");
        chatInput->setMaximumHeight(100);
        chatLayout->addWidget(chatInput);
        
        QPushButton *sendButton = new QPushButton("Send", this);
        connect(sendButton, &QPushButton::clicked, this, &GeminiWidget::sendMessage);
        chatLayout->addWidget(sendButton);
        
        // Add all components to main layout
        layout->addWidget(configGroup);
        layout->addWidget(chatGroup);
        
        // Set up network manager for API requests
        networkManager = new QNetworkAccessManager(this);
        connect(networkManager, &QNetworkAccessManager::finished, this, &GeminiWidget::handleApiResponse);
        
        // Load saved configuration
        loadConfig();
    }

private slots:
    void saveConfig() {
        QSettings settings("MyDevApp", "GeminiAPI");
        settings.setValue("apiKey", apiKeyInput->text());
        settings.setValue("model", modelSelector->currentText());
        
        chatOutput->append("<i>Configuration saved.</i>");
    }
    
    void loadConfig() {
        QSettings settings("MyDevApp", "GeminiAPI");
        apiKeyInput->setText(settings.value("apiKey").toString());
        
        QString savedModel = settings.value("model").toString();
        if (!savedModel.isEmpty()) {
            int index = modelSelector->findText(savedModel);
            if (index >= 0) {
                modelSelector->setCurrentIndex(index);
            }
        }
    }
    
    void sendMessage() {
        QString message = chatInput->toPlainText().trimmed();
        if (message.isEmpty()) return;
        
        chatOutput->append("<b>You:</b> " + message);
        chatInput->clear();
        
        // Create API request
        QString apiKey = apiKeyInput->text();
        if (apiKey.isEmpty()) {
            chatOutput->append("<span style='color: red;'>Error: API key is required!</span>");
            return;
        }
        
        QString model = modelSelector->currentText();
        QString endpoint = "https://generativelanguage.googleapis.com/v1/models/" + model + ":generateContent?key=" + apiKey;
        
        QNetworkRequest request(QUrl(endpoint));
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        
        QJsonObject requestBody;
        QJsonArray contents;
        QJsonObject content;
        content["role"] = "user";
        content["parts"] = QJsonArray{QJsonObject{{"text", message}}};
        contents.append(content);
        requestBody["contents"] = contents;
        
        QJsonDocument doc(requestBody);
        
        chatOutput->append("<i>Waiting for Gemini response...</i>");
        networkManager->post(request, doc.toJson());
    }
    
    void handleApiResponse(QNetworkReply *reply) {
        if (reply->error() != QNetworkReply::NoError) {
            chatOutput->append("<span style='color: red;'>Error: " + reply->errorString() + "</span>");
            reply->deleteLater();
            return;
        }
        
        QByteArray responseData = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(responseData);
        
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            
            if (obj.contains("candidates")) {
                QJsonArray candidates = obj["candidates"].toArray();
                if (!candidates.isEmpty()) {
                    QJsonObject candidate = candidates.first().toObject();
                    QJsonObject content = candidate["content"].toObject();
                    QJsonArray parts = content["parts"].toArray();
                    if (!parts.isEmpty()) {
                        QString text = parts.first().toObject()["text"].toString();
                        chatOutput->append("<b>Gemini:</b> " + text);
                    }
                }
            } else if (obj.contains("error")) {
                QJsonObject error = obj["error"].toObject();
                chatOutput->append("<span style='color: red;'>API Error: " + 
                                  error["message"].toString() + "</span>");
            }
        }
        
        reply->deleteLater();
    }

private:
    QLineEdit *apiKeyInput;
    QComboBox *modelSelector;
    QPushButton *saveConfigButton;
    QTextEdit *chatOutput;
    QTextEdit *chatInput;
    QNetworkAccessManager *networkManager;
};

// Main application window
class MainWindow : public QMainWindow {
public:
    MainWindow() : QMainWindow() {
        setWindowTitle("Development Environment");
        resize(1200, 800);
        
        // Create central widget and main splitter
        QWidget *centralWidget = new QWidget(this);
        QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
        
        QSplitter *mainSplitter = new QSplitter(Qt::Horizontal, this);
        
        // IDE panel
        QWidget *idePanel = new QWidget(this);
        QVBoxLayout *ideLayout = new QVBoxLayout(idePanel);
        
        QLabel *ideTitle = new QLabel("Code Editor", this);
        ideTitle->setStyleSheet("font-weight: bold; font-size: 14px;");
        
        CodeEditor *codeEditor = new CodeEditor(this);
        codeEditor->setPlaceholderText("Write your code here...");
        
        ideLayout->addWidget(ideTitle);
        ideLayout->addWidget(codeEditor);
        
        // Gemini API panel
        QWidget *geminiPanel = new QWidget(this);
        QVBoxLayout *geminiLayout = new QVBoxLayout(geminiPanel);
        
        QLabel *geminiTitle = new QLabel("Gemini AI", this);
        geminiTitle->setStyleSheet("font-weight: bold; font-size: 14px;");
        
        GeminiWidget *geminiWidget = new GeminiWidget(this);
        
        geminiLayout->addWidget(geminiTitle);
        geminiLayout->addWidget(geminiWidget);
        
        // Terminal panel
        QWidget *terminalPanel = new QWidget(this);
        QVBoxLayout *terminalLayout = new QVBoxLayout(terminalPanel);
        
        QLabel *terminalTitle = new QLabel("Terminal", this);
        terminalTitle->setStyleSheet("font-weight: bold; font-size: 14px;");
        
        TerminalWidget *terminal = new TerminalWidget(this);
        
        terminalLayout->addWidget(terminalTitle);
        terminalLayout->addWidget(terminal);
        
        // Add panels to splitter
        mainSplitter->addWidget(idePanel);
        mainSplitter->addWidget(geminiPanel);
        mainSplitter->addWidget(terminalPanel);
        mainSplitter->setSizes(QList<int>() << 400 << 400 << 400);
        
        mainLayout->addWidget(mainSplitter);
        setCentralWidget(centralWidget);
        
        // Create menu bar
        setupMenus();
        
        // Create status bar
        statusBar()->showMessage("Ready");
    }

private:
    void setupMenus() {
        // File menu
        QMenu *fileMenu = menuBar()->addMenu("&File");
        
        QAction *newAction = fileMenu->addAction("&New");
        newAction->setShortcut(QKeySequence::New);
        
        QAction *openAction = fileMenu->addAction("&Open");
        openAction->setShortcut(QKeySequence::Open);
        
        QAction *saveAction = fileMenu->addAction("&Save");
        saveAction->setShortcut(QKeySequence::Save);
        
        fileMenu->addSeparator();
        
        QAction *exitAction = fileMenu->addAction("E&xit");
        exitAction->setShortcut(QKeySequence::Quit);
        connect(exitAction, &QAction::triggered, this, &MainWindow::close);
        
        // Edit menu
        QMenu *editMenu = menuBar()->addMenu("&Edit");
        
        QAction *undoAction = editMenu->addAction("&Undo");
        undoAction->setShortcut(QKeySequence::Undo);
        
        QAction *redoAction = editMenu->addAction("&Redo");
        redoAction->setShortcut(QKeySequence::Redo);
        
        editMenu->addSeparator();
        
        QAction *cutAction = editMenu->addAction("Cu&t");
        cutAction->setShortcut(QKeySequence::Cut);
        
        QAction *copyAction = editMenu->addAction("&Copy");
        copyAction->setShortcut(QKeySequence::Copy);
        
        QAction *pasteAction = editMenu->addAction("&Paste");
        pasteAction->setShortcut(QKeySequence::Paste);
        
        // View menu
        QMenu *viewMenu = menuBar()->addMenu("&View");
        
        QAction *toggleTerminalAction = viewMenu->addAction("Toggle &Terminal");
        toggleTerminalAction->setCheckable(true);
        toggleTerminalAction->setChecked(true);
        
        QAction *toggleGeminiAction = viewMenu->addAction("Toggle &Gemini Panel");
        toggleGeminiAction->setCheckable(true);
        toggleGeminiAction->setChecked(true);
    }
};

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("Development Environment");
    
    MainWindow mainWindow;
    mainWindow.show();
    
    return app.exec();
}

#include "main.moc"
