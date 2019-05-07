#ifndef MAINSERVER_H_
#define MAINSERVER_H_

#include <QReadWriteLock>
#include <QStringList>
#include <QRunnable>
#include <QEvent>
#include <QMutex>
#include <QHash>
#include <QMap>

#include <vector>
using namespace std;

#include "tv.h"
#include "playbacksock.h"
#include "mthreadpool.h"
#include "encoderlink.h"
#include "exitcodes.h"
#include "filetransfer.h"
#include "scheduler.h"
#include "livetvchain.h"
#include "autoexpire.h"
#include "mythsocket.h"
#include "mythdeque.h"
#include "mythdownloadmanager.h"

#ifdef DeleteFile
#undef DeleteFile
#endif

class QUrl;
class MythServer;
class QTimer;
class FileSystemInfo;
class MetadataFactory;
class FreeSpaceUpdater;

class DeleteStruct 
{
    friend class MainServer;
  public:
    DeleteStruct(MainServer *ms, const QString& filename, const QString& title,
                 uint chanid, QDateTime recstartts, QDateTime recendts,
                 uint recordedId,
                 bool forceMetadataDelete) : 
        m_ms(ms), m_filename(filename), m_title(title), 
        m_chanid(chanid), m_recstartts(recstartts), 
        m_recendts(recendts), m_recordedid(recordedId),
        m_forceMetadataDelete(forceMetadataDelete)
    {
    }

    DeleteStruct(MainServer *ms, const QString& filename, int fd, off_t size) :
        m_ms(ms), m_filename(filename), m_fd(fd), m_size(size)
    {
    }

  protected:
    MainServer *m_ms                  {nullptr};
    QString     m_filename;
    QString     m_title;
    uint        m_chanid              {0};
    QDateTime   m_recstartts;
    QDateTime   m_recendts;
    uint        m_recordedid          {0};
    bool        m_forceMetadataDelete {false};
    int         m_fd                  {-1};
    off_t       m_size                {0};
};

class DeleteThread : public QRunnable, public DeleteStruct
{
  public:
    DeleteThread(MainServer *ms, const QString& filename, const QString& title, uint chanid,
                 QDateTime recstartts, QDateTime recendts, uint recordingId,
                 bool forceMetadataDelete) :
                     DeleteStruct(ms, filename, title, chanid, recstartts,
                                  recendts, recordingId, forceMetadataDelete)  {}
    void start(void)
        { MThreadPool::globalInstance()->startReserved(this, "DeleteThread"); }
    void run(void) override; // QRunnable
};

class TruncateThread : public QRunnable, public DeleteStruct
{
  public:
    TruncateThread(MainServer *ms, const QString& filename, int fd, off_t size) :
                DeleteStruct(ms, filename, fd, size)  {}
    void start(void)
        { MThreadPool::globalInstance()->start(this, "Truncate"); }
    void run(void) override; // QRunnable
};

class RenameThread : public QRunnable
{
public:
    RenameThread(MainServer &ms, PlaybackSock &pbs,
                 const QString& src, const QString& dst)
        : m_ms(ms), m_pbs(pbs), m_src(src), m_dst(dst) {}
    void run(void) override; // QRunnable

private:
    static QMutex s_renamelock;

    MainServer   &m_ms;
    PlaybackSock &m_pbs;
    QString       m_src, m_dst;
};

class MainServer : public QObject, public MythSocketCBs
{
    Q_OBJECT

    friend class DeleteThread;
    friend class TruncateThread;
    friend class FreeSpaceUpdater;
    friend class RenameThread;
  public:
    MainServer(bool master, int port,
               QMap<int, EncoderLink *> *tvList,
               Scheduler *sched, AutoExpire *expirer);

    ~MainServer();

    void Stop(void);

    void customEvent(QEvent *e) override; // QObject

    bool isClientConnected(bool onlyBlockingClients = false);
    void ShutSlaveBackendsDown(QString &haltcmd);

    void ProcessRequest(MythSocket *sock);

    void readyRead(MythSocket *socket) override; // MythSocketCBs
    void connectionClosed(MythSocket *socket) override; // MythSocketCBs
    void connectionFailed(MythSocket *socket) override // MythSocketCBs
        { (void)socket; }
    void connected(MythSocket *socket) override // MythSocketCBs
        { (void)socket; }

    void DeletePBS(PlaybackSock *sock);

    size_t GetCurrentMaxBitrate(void);
    void BackendQueryDiskSpace(QStringList &strlist, bool consolidated,
                               bool allHosts);
    void GetFilesystemInfos(QList<FileSystemInfo> &fsInfos,
                            bool useCache=true);

    int GetExitCode() const { return m_exitCode; }

    void UpdateSystemdStatus(void);

  protected slots:
    void reconnectTimeout(void);
    void deferredDeleteSlot(void);
    void autoexpireUpdate(void);

  private slots:
    void NewConnection(qt_socket_fd_t socketDescriptor);

  private:

    void ProcessRequestWork(MythSocket *sock);
    void HandleAnnounce(QStringList &slist, QStringList commands,
                        MythSocket *socket);
    void HandleDone(MythSocket *socket);

    void GetActiveBackends(QStringList &hosts);
    void HandleActiveBackendsQuery(PlaybackSock *pbs);
    void HandleIsActiveBackendQuery(QStringList &slist, PlaybackSock *pbs);
    void HandleMoveFile(PlaybackSock *pbs, const QString &storagegroup,
                        const QString &src, const QString &dst);
    bool HandleDeleteFile(QStringList &slist, PlaybackSock *pbs);
    bool HandleDeleteFile(const QString& filename, const QString& storagegroup,
                          PlaybackSock *pbs = nullptr);
    void HandleQueryRecordings(const QString& type, PlaybackSock *pbs);
    void HandleQueryRecording(QStringList &slist, PlaybackSock *pbs);
    void HandleStopRecording(QStringList &slist, PlaybackSock *pbs);
    void DoHandleStopRecording(RecordingInfo &recinfo, PlaybackSock *pbs);
    void HandleDeleteRecording(QString &chanid, QString &starttime,
                               PlaybackSock *pbs, bool forceMetadataDelete,
                               bool forgetHistory);
    void HandleDeleteRecording(QStringList &slist, PlaybackSock *pbs,
                               bool forceMetadataDelete);
    void DoHandleDeleteRecording(RecordingInfo &recinfo, PlaybackSock *pbs,
                                 bool forceMetadataDelete, bool expirer=false,
                                 bool forgetHistory=false);
    void HandleUndeleteRecording(QStringList &slist, PlaybackSock *pbs);
    void DoHandleUndeleteRecording(RecordingInfo &recinfo, PlaybackSock *pbs);
    void HandleForgetRecording(QStringList &slist, PlaybackSock *pbs);
    void HandleRescheduleRecordings(const QStringList &request, 
                                    PlaybackSock *pbs);
    bool HandleAddChildInput(uint inputid);
    void HandleGoToSleep(PlaybackSock *pbs);
    void HandleQueryFreeSpace(PlaybackSock *pbs, bool allHosts);
    void HandleQueryFreeSpaceSummary(PlaybackSock *pbs);
    void HandleQueryCheckFile(QStringList &slist, PlaybackSock *pbs);
    void HandleQueryFileExists(QStringList &slist, PlaybackSock *pbs);
    void HandleQueryFindFile(QStringList &slist, PlaybackSock *pbs);
    void HandleQueryFileHash(QStringList &slist, PlaybackSock *pbs);
    void HandleQueryGuideDataThrough(PlaybackSock *pbs);
    void HandleGetPendingRecordings(PlaybackSock *pbs, const QString& table = "", int recordid=-1);
    void HandleGetScheduledRecordings(PlaybackSock *pbs);
    void HandleGetConflictingRecordings(QStringList &slist, PlaybackSock *pbs);
    void HandleGetExpiringRecordings(PlaybackSock *pbs);
    void HandleSGGetFileList(QStringList &sList, PlaybackSock *pbs);
    void HandleSGFileQuery(QStringList &sList, PlaybackSock *pbs);
    void HandleGetFreeInputInfo(PlaybackSock *pbs, uint excluded_input);
    void HandleGetNextFreeRecorder(QStringList &slist, PlaybackSock *pbs);
    void HandleGetFreeRecorder(PlaybackSock *pbs);
    void HandleGetFreeRecorderCount(PlaybackSock *pbs);
    void HandleGetFreeRecorderList(PlaybackSock *pbs);
    void HandleGetConnectedRecorderList(PlaybackSock *pbs);
    void HandleRecorderQuery(QStringList &slist, QStringList &commands,
                             PlaybackSock *pbs);
    void HandleSetNextLiveTVDir(QStringList &commands, PlaybackSock *pbs);
    void HandleFileTransferQuery(QStringList &slist, QStringList &commands,
                                 PlaybackSock *pbs);
    void HandleGetRecorderNum(QStringList &slist, PlaybackSock *pbs);
    void HandleGetRecorderFromNum(QStringList &slist, PlaybackSock *pbs);
    void HandleMessage(QStringList &slist, PlaybackSock *pbs);
    void HandleSetVerbose(QStringList &slist, PlaybackSock *pbs);
    void HandleSetLogLevel(QStringList &slist, PlaybackSock *pbs);
    void HandleGenPreviewPixmap(QStringList &slist, PlaybackSock *pbs);
    void HandlePixmapLastModified(QStringList &slist, PlaybackSock *pbs);
    void HandlePixmapGetIfModified(const QStringList &slist, PlaybackSock *pbs);
    void HandleIsRecording(QStringList &slist, PlaybackSock *pbs);
    void HandleCheckRecordingActive(QStringList &slist, PlaybackSock *pbs);
    void HandleFillProgramInfo(QStringList &slist, PlaybackSock *pbs);
    void HandleSetChannelInfo(QStringList &slist, PlaybackSock *pbs);
    void HandleRemoteEncoder(QStringList &slist, QStringList &commands,
                             PlaybackSock *pbs);
    void HandleLockTuner(PlaybackSock *pbs, int cardid = -1);
    void HandleFreeTuner(int cardid, PlaybackSock *pbs);
    void HandleCutMapQuery(const QString &chanid, const QString &starttime,
                           PlaybackSock *pbs, bool commbreak);
    void HandleCommBreakQuery(const QString &chanid, const QString &starttime,
                              PlaybackSock *pbs);
    void HandleCutlistQuery(const QString &chanid, const QString &starttime,
                            PlaybackSock *pbs);
    void HandleBookmarkQuery(const QString &chanid, const QString &starttime,
                             PlaybackSock *pbs);
    void HandleSetBookmark(QStringList &tokens, PlaybackSock *pbs);
    void HandleSettingQuery(QStringList &tokens, PlaybackSock *pbs);
    void HandleSetSetting(QStringList &tokens, PlaybackSock *pbs);
    void HandleScanVideos(PlaybackSock *pbs);
    void HandleScanMusic(const QStringList &slist, PlaybackSock *pbs);
    void HandleMusicTagUpdateVolatile(const QStringList &slist, PlaybackSock *pbs);
    void HandleMusicTagUpdateMetadata(const QStringList &slist, PlaybackSock *pbs);
    void HandleMusicFindAlbumArt(const QStringList &slist, PlaybackSock *pbs);
    void HandleMusicTagGetImage(const QStringList &slist, PlaybackSock *pbs);
    void HandleMusicTagAddImage(const QStringList &slist, PlaybackSock *pbs);
    void HandleMusicTagRemoveImage(const QStringList &slist, PlaybackSock *pbs);
    void HandleMusicTagChangeImage(const QStringList &slist, PlaybackSock *pbs);
    void HandleMusicCalcTrackLen(const QStringList &slist, PlaybackSock *pbs);
    void HandleMusicFindLyrics(const QStringList &slist, PlaybackSock *pbs);
    void HandleMusicGetLyricGrabbers(const QStringList &slist, PlaybackSock *pbs);
    void HandleMusicSaveLyrics(const QStringList &slist, PlaybackSock *pbs);
    void HandleVersion(MythSocket *socket, const QStringList &slist);
    void HandleBackendRefresh(MythSocket *socket);
    void HandleQueryLoad(PlaybackSock *pbs);
    void HandleQueryUptime(PlaybackSock *pbs);
    void HandleQueryHostname(PlaybackSock *pbs);
    void HandleQueryMemStats(PlaybackSock *pbs);
    void HandleQueryTimeZone(PlaybackSock *pbs);
    void HandleBlockShutdown(bool blockShutdown, PlaybackSock *pbs);
    void HandleDownloadFile(const QStringList &command, PlaybackSock *pbs);
    void HandleSlaveDisconnectedEvent(const MythEvent &event);

    void SendResponse(MythSocket *sock, QStringList &commands);
    void SendErrorResponse(MythSocket *sock, const QString &error);
    void SendErrorResponse(PlaybackSock *pbs, const QString &error);
    void SendSlaveDisconnectedEvent(const QList<uint> &offlineEncoderIDs,
                                    bool needsReschedule);

    void getGuideDataThrough(QDateTime &GuideDataThrough);

    PlaybackSock *GetSlaveByHostname(const QString &hostname);
    PlaybackSock *GetMediaServerByHostname(const QString &hostname);
    PlaybackSock *GetPlaybackBySock(MythSocket *socket);
    FileTransfer *GetFileTransferByID(int id);
    FileTransfer *GetFileTransferBySock(MythSocket *socket);

    QString LocalFilePath(const QString &path, const QString &wantgroup);

    int GetfsID(const QList<FileSystemInfo>::iterator& fsInfo);

    void DoTruncateThread(DeleteStruct *ds);
    void DoDeleteThread(DeleteStruct *ds);
    void DeleteRecordedFiles(DeleteStruct *ds);
    void DoDeleteInDB(DeleteStruct *ds);

    LiveTVChain *GetExistingChain(const QString &id);
    LiveTVChain *GetExistingChain(const MythSocket *sock);
    LiveTVChain *GetChainWithRecording(const ProgramInfo &pginfo);
    void AddToChains(LiveTVChain *chain);
    void DeleteChain(LiveTVChain *chain);

    void SetExitCode(int exitCode, bool closeApplication);

    static int  DeleteFile(const QString &filename, bool followLinks,
                           bool deleteBrokenSymlinks = false);
    static int  OpenAndUnlink(const QString &filename);
    static bool TruncateAndClose(ProgramInfo *pginfo,
                                 int fd, const QString &filename,
                                 off_t fsize);

    vector<LiveTVChain*> m_liveTVChains;
    QMutex               m_liveTVChainsLock;

    QMap<int, EncoderLink *> *m_encoderList  {nullptr};

    MythServer            *m_mythserver      {nullptr};
    MetadataFactory       *m_metadatafactory {nullptr};

    QReadWriteLock         m_sockListLock;
    vector<PlaybackSock *> m_playbackList;
    vector<FileTransfer *> m_fileTransferList;
    QSet<MythSocket*>      m_controlSocketList;
    vector<MythSocket*>    m_decrRefSocketList;

    QMutex                      m_masterFreeSpaceListLock;
    FreeSpaceUpdater * volatile m_masterFreeSpaceListUpdater {nullptr};
    QWaitCondition              m_masterFreeSpaceListWait;
    QStringList                 m_masterFreeSpaceList;

    QTimer       *m_masterServerReconnect    {nullptr}; // audited ref #5318
    PlaybackSock *m_masterServer             {nullptr};

    bool m_ismaster;

    QMutex m_deletelock;
    MThreadPool m_threadPool;

    bool m_masterBackendOverride             {false};

    Scheduler  *m_sched                      {nullptr};
    AutoExpire *m_expirer                    {nullptr};
    QMutex      m_addChildInputLock;

    struct DeferredDeleteStruct
    {
        PlaybackSock *sock;
        QDateTime ts;
    };

    QMutex  m_deferredDeleteLock;
    QTimer *m_deferredDeleteTimer            {nullptr}; // audited ref #5318
    MythDeque<DeferredDeleteStruct> m_deferredDeleteList;

    QTimer *m_autoexpireUpdateTimer          {nullptr}; // audited ref #5318
    static QMutex s_truncate_and_close_lock;

    QMap<QString, int>    m_fsIDcache;
    QMutex                m_fsIDcacheLock;
    QList<FileSystemInfo> m_fsInfosCache;
    QMutex                m_fsInfosCacheLock;

    QMutex                     m_downloadURLsLock;
    QMap<QString, QString>     m_downloadURLs;

    int m_exitCode                           {GENERIC_EXIT_OK};

    typedef QHash<QString,QString> RequestedBy;
    RequestedBy                m_previewRequestedBy;

    bool m_stopped                           {false};

    static const uint kMasterServerReconnectTimeout;
};

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */
