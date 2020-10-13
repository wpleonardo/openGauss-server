/*
 * Portions Copyright (c) 2020 Huawei Technologies Co.,Ltd.
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * openGauss is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *
 *          http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * ---------------------------------------------------------------------------------------
 * 
 * knl_session.h
 *        Data stucture for session level global variables.
 *
 * several guidelines for put variables in knl_session.h
 *
 * variables which related to session only and can be decoupled with thread status 
 * should put int knl_session.h
 *
 * pay attention to memory allocation
 * allocate runtime session variable in session context, which is highly recommended
 * to do that.
 *
 * variables related to dedicated utility thread(wal, bgwrited, log...) usually put
 * in knl_thread.h
 *
 * all context define below should follow naming rules:
 * knl_u_sess->on_xxxx
 * 
 * IDENTIFICATION
 *        src/include/knl/knl_session.h
 *
 * ---------------------------------------------------------------------------------------
 */

#ifndef SRC_INCLUDE_KNL_KNL_SESSION_H_
#define SRC_INCLUDE_KNL_KNL_SESSION_H_

#include <signal.h>

#include "c.h"
#include "datatype/timestamp.h"
#include "gs_thread.h"
#include "knl/knl_guc.h"
#include "lib/dllist.h"
#include "lib/ilist.h"
#include "lib/stringinfo.h"
#include "libpq/pqcomm.h"
#include "nodes/pg_list.h"
#include "cipher.h"
#include "openssl/ossl_typ.h"
#include "portability/instr_time.h"
#include "storage/backendid.h"
#include "storage/s_lock.h"
#include "storage/shmem.h"
#include "postmaster/bgworker.h"
#include "storage/dsm.h"
#include "utils/palloc.h"

typedef void (*pg_on_exit_callback)(int code, Datum arg);


/* all session level attribute which expose to user. */
typedef struct knl_session_attr {
    knl_session_attr_sql attr_sql;
    knl_session_attr_storage attr_storage;
    knl_session_attr_security attr_security;
    knl_session_attr_network attr_network;
    knl_session_attr_memory attr_memory;
    knl_session_attr_resource attr_resource;
    knl_session_attr_common attr_common;
} knl_session_attr;

typedef struct knl_u_stream_context {
    uint32 producer_dop;

    uint32 smp_id;

    bool in_waiting_quit;

    bool dummy_thread;

    bool enter_sync_point;

    bool inStreamEnv;

    bool stop_mythread;

    ThreadId stop_pid;

    uint64 stop_query_id;

    class StreamNodeGroup* global_obj;

    class StreamProducer* producer_obj;
} knl_u_stream_context;

typedef struct knl_u_executor_context {
    List* remotequery_list;

    bool exec_result_checkqual_fail;

    bool executor_stop_flag;

    bool under_stream_runtime;

    /* This variable indicates wheather the prescribed extension is supported */
    bool extension_is_valid;

    uint2* global_bucket_map;

    struct HTAB* vec_func_hash;

    struct PartitionIdentifier* route;

    struct TupleHashTableData* cur_tuple_hash_table;

    class lightProxy* cur_light_proxy_obj;

    /*
     * ActivePortal is the currently executing Portal (the most closely nested,
     * if there are several).
     */
    struct PortalData* ActivePortal;
    bool need_track_resource;

    HTAB* PortalHashTable;
    unsigned int unnamed_portal_count;
    /* the single instance for statement retry controll per thread */
    struct StatementRetryController* RetryController;

    bool hasTempObject;
    bool DfsDDLIsTopLevelXact;
    /* global variable used to determine if we could cancel redistribution transaction */
    bool could_cancel_redistribution;

    bool g_pgaudit_agent_attached;
    /* whether to track sql stats */
    bool pgaudit_track_sqlddl;

    struct HashScanListData* HashScans;

    /* the flag indicate the executor can stop, do not send anything to outer */
    bool executorStopFlag;

    struct OpFusion* CurrentOpFusionObj;

    bool is_exec_trigger_func;
} knl_u_executor_context;

typedef struct knl_u_sig_context {
    /*
     * Flag to mark SIGHUP. Whenever the main loop comes around it
     * will reread the configuration file. (Better than doing the
     * reading in the signal handler, ey?)
     */
    volatile sig_atomic_t got_SIGHUP;

    /*
     * like got_PoolReload, but just for the compute pool.
     * see CPmonitor_MainLoop for more details.
     */
    volatile sig_atomic_t got_PoolReload;
    volatile sig_atomic_t cp_PoolReload;
} knl_u_sig_context;

typedef struct knl_u_SPI_context {
#define BUFLEN 64

    Oid lastoid;

    char buf[BUFLEN];

    int _stack_depth; /* allocated size of _SPI_stack */

    int _connected;

    int _curid;

    struct _SPI_connection* _stack;

    struct _SPI_connection* _current;

    bool is_toplevel_stp;

    bool is_stp;

    bool is_proconfig_set;

    int portal_stp_exception_counter;
} knl_u_SPI_context;

typedef struct knl_u_index_context {
    typedef uint64 XLogRecPtr;
    XLogRecPtr counter;
} knl_u_index_context;

typedef struct knl_u_instrument_context {
    bool perf_monitor_enable;

    bool can_record_to_table;

    int operator_plan_number;

    bool OBS_instr_valid;

    bool* p_OBS_instr_valid;

    class StreamInstrumentation* global_instr;

    class ThreadInstrumentation* thread_instr;

    class OBSInstrumentation* obs_instr;

    struct Qpid* gs_query_id;

    struct BufferUsage* pg_buffer_usage;
} knl_u_instrument_context;

typedef struct knl_u_analyze_context {
    bool is_under_analyze;

    bool need_autoanalyze;

    MemoryContext analyze_context;

    struct AutoAnaProcess* autoanalyze_process;

    struct StringInfoData* autoanalyze_timeinfo;

    struct BufferAccessStrategyData* vac_strategy;
} knl_u_analyze_context;

#define PATH_SEED_FACTOR_LEN 3

typedef struct knl_u_optimizer_context {
    bool disable_dop_change;

    bool enable_nodegroup_explain;

    bool has_obsrel;

    bool is_stream;

    bool is_stream_support;

    bool is_multiple_nodegroup_scenario;

    bool is_all_in_installation_nodegroup_scenario;

    bool is_randomfunc_shippable;

    int srvtype;

    int qrw_inlist2join_optmode;

    uint32 plan_current_seed;

    uint32 plan_prev_seed;

    uint16 path_current_seed_factor[PATH_SEED_FACTOR_LEN];

    double cursor_tuple_fraction;

    /* Global variables used for parallel execution. */
    int query_dop_store; /* Store the dop. */

    int query_dop; /* Degree of parallel, 1 means not parallel. */

    double smp_thread_cost;

    /* u_sess->opt_cxt.max_query_dop:
     * When used in dynamic smp, this variable set the max limit of dop
     * <0 means turned off dynsmp.
     *    0 means optimized dop based on query and resources
     *    1 is the same as set u_sess->opt_cxt.query_dop=1
     *    2..n means max limit of dop
     */
    int max_query_dop;

    int parallel_debug_mode; /* Control the parallel debug mode. */

    int skew_strategy_opt;

    int op_work_mem;

    MemoryContext ft_context;

    struct Distribution* in_redistribution_group_distribution;

    struct Distribution* compute_permission_group_distribution;

    struct Distribution* query_union_set_group_distribution;

    struct DynamicSmpInfo* dynamic_smp_info;

    struct PartitionIdentifier* bottom_seq;

    struct PartitionIdentifier* top_seq;

    struct HTAB* opr_proof_cache_hash;

    struct ShippingInfo* not_shipping_info;
} knl_u_optimizer_context;

typedef struct knl_u_parser_context {
    bool eaten_begin;

    bool eaten_declare;

    bool has_dollar;

    bool has_placeholder;

    bool stmt_contains_operator_plus;

    List* hint_list;

    List* hint_warning;

    struct HTAB* opr_cache_hash;

    void* param_info;

    StringInfo param_message;

    MemoryContext ddl_pbe_context;
} knl_u_parser_context;

typedef struct knl_u_trigger_context {
    struct HTAB* ri_query_cache;

    struct HTAB* ri_compare_cache;

    bool exec_row_trigger_on_datanode;

    /* How many levels deep into trigger execution are we? */
    int MyTriggerDepth;
    List* info_list;
    struct AfterTriggersData* afterTriggers;
} knl_u_trigger_context;

typedef struct knl_u_utils_context {
    char suffix_char;

    Oid suffix_collation;

    int test_err_type;

    /*
     * While compute_array_stats is running, we keep a pointer to the extra data
     * here for use by assorted subroutines.  compute_array_stats doesn't
     * currently need to be re-entrant, so avoiding this is not worth the extra
     * notational cruft that would be needed.
     */
    struct ArrayAnalyzeExtraData* array_extra_data;

    struct ItemPointerData* cur_last_tid;

    struct DistributeTestParam* distribute_test_param;

    struct PGLZ_HistEntry** hist_start;

    struct PGLZ_HistEntry* hist_entries;

    char* analysis_options_configure;

    int* guc_new_value;

    int GUC_check_errcode_value;

    TimestampTz lastFailedLoginTime;

    struct StringInfoData* input_set_message; /* Add for set command in transaction */

    char* GUC_check_errmsg_string;

    char* GUC_check_errdetail_string;

    char* GUC_check_errhint_string;

    HTAB* set_params_htab;

    struct config_generic** sync_guc_variables;

    struct config_bool* ConfigureNamesBool;

    struct config_int* ConfigureNamesInt;

    struct config_real* ConfigureNamesReal;

    struct config_int64* ConfigureNamesInt64;

    struct config_string* ConfigureNamesString;

    struct config_enum* ConfigureNamesEnum;

    int size_guc_variables;

    bool guc_dirty; /* TRUE if need to do commit/abort work */

    bool reporting_enabled; /* TRUE to enable GUC_REPORT */

    int GUCNestLevel; /* 1 when in main transaction */

    unsigned int behavior_compat_flags;

    int save_argc;

    char** save_argv;

    /*
     * In common cases the same roleid (ie, the session or current ID) will
     * be queried repeatedly.  So we maintain a simple one-entry cache for
     * the status of the last requested roleid.  The cache can be flushed
     * at need by watching for cache update events on pg_authid.
     */
    Oid last_roleid; /* InvalidOid == cache not valid */

    bool last_roleid_is_super;

    bool last_roleid_is_sysdba;

    bool last_roleid_is_securityadmin; /* Indicates whether a security admin */

    bool last_roleid_is_auditadmin; /* Indicates whether an audit admin */

    bool roleid_callback_registered;

    /* Hash table to lookup combo cids by cmin and cmax */
    HTAB* comboHash;

    /*
     * An array of cmin,cmax pairs, indexed by combo command id.
     * To convert a combo cid to cmin and cmax, you do a simple array lookup.
     */
    struct ComboCidKeyData* comboCids;

    /* use for stream producer thread */
    struct ComboCidKeyData* StreamParentComboCids;

    int usedComboCids; /* number of elements in comboCids */

    int sizeComboCids; /* allocated size of array */

    int StreamParentsizeComboCids; /* allocated size of array for  StreamParentComboCids */

    /*
     * CurrentSnapshot points to the only snapshot taken in transaction-snapshot
     * mode, and to the latest one taken in a read-committed transaction.
     * SecondarySnapshot is a snapshot that's always up-to-date as of the current
     * instant, even in transaction-snapshot mode.    It should only be used for
     * special-purpose code (say, RI checking.)
     *
     * These SnapshotData structs are static to simplify memory allocation
     * (see the hack in GetSnapshotData to avoid repeated malloc/free).
     */
    struct SnapshotData* CurrentSnapshotData;

    struct SnapshotData* SecondarySnapshotData;

    struct SnapshotData* CurrentSnapshot;

    struct SnapshotData* SecondarySnapshot;

    struct SnapshotData* CatalogSnapshot;

    struct SnapshotData* HistoricSnapshot;

    /* Staleness detection for CatalogSnapshot. */
    bool CatalogSnapshotStale;

    /*
     * These are updated by GetSnapshotData.  We initialize them this way
     * for the convenience of TransactionIdIsInProgress: even in bootstrap
     * mode, we don't want it to say that BootstrapTransactionId is in progress.
     *
     * RecentGlobalXmin and RecentGlobalDataXmin are initialized to
     * InvalidTransactionId, to ensure that no one tries to use a stale
     * value. Readers should ensure that it has been set to something else
     * before using it.
     */
    TransactionId TransactionXmin;

    TransactionId RecentXmin;

    TransactionId RecentGlobalXmin;

    TransactionId RecentGlobalDataXmin;

    /* Global snapshot data */
    bool cn_xc_maintain_mode;
    int snapshot_source;
    TransactionId gxmin;
    TransactionId gxmax;
    uint32 GtmTimeline;
    struct GTM_SnapshotData* g_GTM_Snapshot;
    uint64 g_snapshotcsn;
    bool snapshot_need_sync_wait_all;

    /* (table, ctid) => (cmin, cmax) mapping during timetravel */
    HTAB* tuplecid_data;

    /* Top of the stack of active snapshots */
    struct ActiveSnapshotElt* ActiveSnapshot;

    /*
     * How many snapshots is resowner.c tracking for us?
     *
     * Note: for now, a simple counter is enough.  However, if we ever want to be
     * smarter about advancing our MyPgXact->xmin we will need to be more
     * sophisticated about this, perhaps keeping our own list of snapshots.
     */
    int RegisteredSnapshots;

    /* first GetTransactionSnapshot call in a transaction? */
    bool FirstSnapshotSet;

    /*
     * Remember the serializable transaction snapshot, if any.    We cannot trust
     * FirstSnapshotSet in combination with IsolationUsesXactSnapshot(), because
     * GUC may be reset before us, changing the value of IsolationUsesXactSnapshot.
     */
    struct SnapshotData* FirstXactSnapshot;

    /* Current xact's exported snapshots (a list of Snapshot structs) */
    List* exportedSnapshots;

    uint8_t g_output_version; /* Set the default output schema. */

    int XactIsoLevel;

    /* a string list parsed from GUC variable (uuncontrolled_memory_context),see more @guc.cpp */
    struct memory_context_list* memory_context_limited_white_list;

#ifdef ENABLE_QUNIT
    int qunit_case_number;
#endif

    bool enable_memory_context_control;
} knl_u_utils_context;

typedef struct knl_u_commands_context {
    /* Tablespace usage  information management struct */
    struct TableSpaceUsageStruct* TableSpaceUsageArray;
    bool isUnderCreateForeignTable;
    bool isUnderRefreshMatview;

    Oid CurrentExtensionObject;

    List* PendingLibraryDeletes;
    TransactionId OldestXmin;
    TransactionId FreezeLimit;
    struct SeqTableData* seqtab; /* Head of list of SeqTable items */
    /*
     * last_used_seq is updated by nextval() to point to the last used
     * sequence.
     */
    struct SeqTableData* last_used_seq;

    /* Form of the type created during inplace upgrade */
    char TypeCreateType;

    List* label_provider_list;
    /* bulkload_compatible_illegal_chars and bulkload_copy_state are to be deleted */
    bool bulkload_compatible_illegal_chars;

    /*
     * This variable ought to be a temperary fix for copy to file encoding bug caused by
     * our modification to PGXC copy procejure.
     */
    struct CopyStateData* bulkload_copy_state;
    int dest_encoding_for_copytofile;
    bool need_transcoding_for_copytofile;
    MemoryContext OBSParserContext;
    List* on_commits;
    bool mergePartition_Freeze;
    TransactionId mergePartition_OldestXmin;
    TransactionId mergePartition_FreezeXid;

    /*
     * Indicate that the top relation is temporary in my session.
     * If it is, also we treat its all subordinate relations as temporary and in my session.
     * For an example, if a column relation is temporary in my session,
     * also its cudesc, cudesc index, delta are temporary and in my session.
     */
    bool topRelatationIsInMyTempSession;
    Node bogus_marker; /* marks conflicting defaults */
} knl_u_commands_context;

typedef struct knl_u_contrib_context {
    /* for assigning cursor numbers and prepared statement numbers */
    unsigned int cursor_number;

    int current_cursor_id;
    int file_format;  // enum DfsFileFormat
    int file_number;
} knl_u_contrib_context;

#define LC_ENV_BUFSIZE (NAMEDATALEN + 20)

typedef struct knl_u_locale_context {
    /* lc_time localization cache */
    char* localized_abbrev_days[7];

    char* localized_full_days[7];

    char* localized_abbrev_months[12];

    char* localized_full_months[12];

    bool cur_lc_conv_valid;

    bool cur_lc_time_valid;

    struct lconv* cur_lc_conv;

    int lc_ctype_result;

    int lc_collate_result;

    struct HTAB* collation_cache;

    struct lconv current_lc_conv;
} knl_u_locale_context;

typedef struct knl_u_log_context {
    char* syslog_ident;

    char* module_logging_configure;
    /*
     * msgbuf is declared as static to save the data to put
     * which can be flushed in next put_line()
     */
    struct StringInfoData* msgbuf;
} knl_u_log_context;

typedef struct knl_u_mb_context {
    bool insertValuesBind_compatible_illegal_chars;

    List* ConvProcList; /* List of ConvProcInfo */

    /*
     * These variables point to the currently active conversion functions,
     * or are NULL when no conversion is needed.
     */
    struct FmgrInfo* ToServerConvProc;

    struct FmgrInfo* ToClientConvProc;

    NameData datcollate; /* LC_COLLATE setting */
    NameData datctype;   /* LC_CTYPE setting */

    /*
     * These variables track the currently selected FE and BE encodings.
     */
    struct pg_enc2name* ClientEncoding;

    struct pg_enc2name* DatabaseEncoding;

    struct pg_enc2name* PlatformEncoding;

    /*
     * During backend startup we can't set client encoding because we (a)
     * can't look up the conversion functions, and (b) may not know the database
     * encoding yet either.  So SetClientEncoding() just accepts anything and
     * remembers it for InitializeClientEncoding() to apply later.
     */
    bool backend_startup_complete;

    int pending_client_encoding;
} knl_u_mb_context;

typedef struct knl_u_plancache_context {
    /*
     * This is the head of the backend's list of "saved" CachedPlanSources (i.e.,
     * those that are in long-lived storage and are examined for sinval events).
     * We thread the structs manually instead of using List cells so that we can
     * guarantee to save a CachedPlanSource without error.
     */
    struct CachedPlanSource* first_saved_plan;

    /*
     * If an unnamed prepared statement exists, it's stored here.
     * We keep it separate from the hashtable kept by commands/prepare.c
     * in order to reduce overhead for short-lived queries.
     */
    struct CachedPlanSource* unnamed_stmt_psrc;

    /* recode if the query contains params, used in pgxc_shippability_walker */
    bool query_has_params;

    /*
     * The hash table in which prepared queries are stored. This is
     * per-backend: query plans are not shared between backends.
     * The keys for this hash table are the arguments to PREPARE and EXECUTE
     * (statement names); the entries are PreparedStatement structs.
     */
    HTAB* prepared_queries;

#ifdef PGXC
    /*
     * The hash table where Datanode prepared statements are stored.
     * The keys are statement names referenced from cached RemoteQuery nodes; the
     * entries are DatanodeStatement structs
     */
    HTAB* datanode_queries;
#endif

    bool gpc_in_ddl;
    bool gpc_remote_msg;
    bool gpc_first_send;
} knl_u_plancache_context;

typedef struct knl_u_typecache_context {
    /* The main type cache hashtable searched by lookup_type_cache */
    struct HTAB* TypeCacheHash;

    struct HTAB* RecordCacheHash;

    struct tupleDesc** RecordCacheArray;

    int32 RecordCacheArrayLen; /* allocated length of array */

    int32 NextRecordTypmod; /* number of entries used */
} knl_u_typecache_context;

typedef struct knl_u_tscache_context {
    struct HTAB* TSParserCacheHash;

    struct TSParserCacheEntry* lastUsedParser;

    struct HTAB* TSDictionaryCacheHash;

    struct TSDictionaryCacheEntry* lastUsedDictionary;

    struct HTAB* TSConfigCacheHash;

    struct TSConfigCacheEntry* lastUsedConfig;

    Oid TSCurrentConfigCache;
} knl_u_tscache_context;

/*
 * Description:
 *        There are three processing modes in POSTGRES.  They are
 * BootstrapProcessing or "bootstrap," InitProcessing or
 * "initialization," and NormalProcessing or "normal."
 *
 * The first two processing modes are used during special times. When the
 * system state indicates bootstrap processing, transactions are all given
 * transaction id "one" and are consequently guaranteed to commit. This mode
 * is used during the initial generation of template databases.
 *
 * Initialization mode: used while starting a backend, until all normal
 * initialization is complete.    Some code behaves differently when executed
 * in this mode to enable system bootstrapping.
 *
 * If a POSTGRES backend process is in normal mode, then all code may be
 * executed normally.
 */

typedef enum ProcessingMode {
    BootstrapProcessing,  /* bootstrap creation of template database */
    InitProcessing,       /* initializing system */
    NormalProcessing,     /* normal processing */
    PostUpgradeProcessing /* Post upgrade to run script */
} ProcessingMode;

/*
 *    node group mode
 *     now we support two mode: common node group and logic cluster group
 */
typedef enum NodeGroupMode {
    NG_UNKNOWN = 0, /* unknown mode */
    NG_COMMON,      /* common node group */
    NG_LOGIC,       /* logic cluster node group */
} NodeGroupMode;

typedef struct knl_u_misc_context {
    enum ProcessingMode Mode;

    /* Note: we rely on this to initialize as zeroes */
    char socketLockFile[MAXPGPATH];
    char hasocketLockFile[MAXPGPATH];

    /* ----------------------------------------------------------------
     *    User ID state
     *
     * We have to track several different values associated with the concept
     * of "user ID".
     *
     * AuthenticatedUserId is determined at connection start and never changes.
     *
     * SessionUserId is initially the same as AuthenticatedUserId, but can be
     * changed by SET SESSION AUTHORIZATION (if AuthenticatedUserIsSuperuser).
     * This is the ID reported by the SESSION_USER SQL function.
     *
     * OuterUserId is the current user ID in effect at the "outer level" (outside
     * any transaction or function).  This is initially the same as SessionUserId,
     * but can be changed by SET ROLE to any role that SessionUserId is a
     * member of.  (XXX rename to something like CurrentRoleId?)
     *
     * CurrentUserId is the current effective user ID; this is the one to use
     * for all normal permissions-checking purposes.  At outer level this will
     * be the same as OuterUserId, but it changes during calls to SECURITY
     * DEFINER functions, as well as locally in some specialized commands.
     *
     * SecurityRestrictionContext holds flags indicating reason(s) for changing
     * CurrentUserId.  In some cases we need to lock down operations that are
     * not directly controlled by privilege settings, and this provides a
     * convenient way to do it.
     * ----------------------------------------------------------------
     */
    Oid AuthenticatedUserId;

    Oid SessionUserId;

    Oid OuterUserId;

    Oid CurrentUserId;

    int SecurityRestrictionContext;

    const char* CurrentUserName;

    /*
     * logic cluster information every backend.
     */
    Oid current_logic_cluster;

    enum NodeGroupMode current_nodegroup_mode;

    bool nodegroup_callback_registered;

    /*
     * Pseudo_CurrentUserId always refers to CurrentUserId, except in executing stored procedure,
     * in that case, Pseudo_CurrentUserId refers to the user id that created the stored procedure.
     */
    Oid* Pseudo_CurrentUserId;

    /* We also have to remember the superuser state of some of these levels */
    bool AuthenticatedUserIsSuperuser;

    bool SessionUserIsSuperuser;

    /* We also remember if a SET ROLE is currently active */
    bool SetRoleIsActive;

    /* Flag telling that we are loading shared_preload_libraries */
    bool process_shared_preload_libraries_in_progress;

    /* Flag telling that authentication already finish */
    bool authentication_finished;
} knl_misc_context;

typedef struct knl_u_proc_context {
    struct Port* MyProcPort;

    Oid MyRoleId;

    Oid MyDatabaseId;

    Oid MyDatabaseTableSpace;

    /*
     * DatabasePath is the path (relative to t_thrd.proc_cxt.DataDir) of my database's
     * primary directory, ie, its directory in the default tablespace.
     */
    char* DatabasePath;

    bool Isredisworker;
    bool IsInnerMaintenanceTools; /* inner tool check flag */
    bool clientIsGsrewind;        /* gs_rewind tool check flag */
    bool clientIsGsredis;         /* gs_redis tool check flag */
    bool clientIsGsdump;          /* gs_dump tool check flag */
    bool clientIsGsCtl;           /* gs_ctl tool check flag */
    bool clientIsGsBasebackup;    /* gs_basebackup tool check flag */
    bool clientIsGsRestore;       /* gs_restore tool check flag */
    bool IsBinaryUpgrade;
    bool IsWLMWhiteList;          /* this proc will not be controled by WLM */
} knl_u_proc_context;

/* maximum possible number of fields in a date string */
#define MAXDATEFIELDS 25

typedef struct knl_u_time_context {
    /*
     * DateOrder defines the field order to be assumed when reading an
     * ambiguous date (anything not in YYYY-MM-DD format, with a four-digit
     * year field first, is taken to be ambiguous):
     *    DATEORDER_YMD specifies field order yy-mm-dd
     *    DATEORDER_DMY specifies field order dd-mm-yy ("European" convention)
     *    DATEORDER_MDY specifies field order mm-dd-yy ("US" convention)
     *
     * In the Postgres and SQL DateStyles, DateOrder also selects output field
     * order: day comes before month in DMY style, else month comes before day.
     *
     * The user-visible "DateStyle" run-time parameter subsumes both of these.
     */
    int DateStyle;

    int DateOrder;

    /*
     * u_sess->time_cxt.HasCTZSet is true if user has set timezone as a numeric offset from UTC.
     * If so, CTimeZone is the timezone offset in seconds (using the Unix-ish
     * sign convention, ie, positive offset is west of UTC, rather than the
     * SQL-ish convention that positive is east of UTC).
     */
    bool HasCTZSet;

    int CTimeZone;

    int sz_timezone_tktbl;

    /*
     * datetktbl holds date/time keywords.
     *
     * Note that this table must be strictly alphabetically ordered to allow an
     * O(ln(N)) search algorithm to be used.
     *
     * The text field is NOT guaranteed to be NULL-terminated.
     *
     * To keep this table reasonably small, we divide the lexval for TZ and DTZ
     * entries by 15 (so they are on 15 minute boundaries) and truncate the text
     * field at TOKMAXLEN characters.
     * Formerly, we divided by 10 rather than 15 but there are a few time zones
     * which are 30 or 45 minutes away from an even hour, most are on an hour
     * boundary, and none on other boundaries.
     *
     * The static table contains no TZ or DTZ entries, rather those are loaded
     * from configuration files and stored in t_thrd.time_cxt.timezone_tktbl, which has the same
     * format as the static datetktbl.
     */
    struct datetkn* timezone_tktbl;

    const struct datetkn* datecache[MAXDATEFIELDS];

    const struct datetkn* deltacache[MAXDATEFIELDS];

    struct HTAB* timezone_cache;
} knl_u_time_context;

typedef struct knl_u_upgrade_context {
    bool InplaceUpgradeSwitch;
    PGDLLIMPORT Oid binary_upgrade_next_etbl_pg_type_oid;
    PGDLLIMPORT Oid binary_upgrade_next_etbl_array_pg_type_oid;
    PGDLLIMPORT Oid binary_upgrade_next_etbl_toast_pg_type_oid;
    PGDLLIMPORT Oid binary_upgrade_next_etbl_heap_pg_class_oid;
    PGDLLIMPORT Oid binary_upgrade_next_etbl_index_pg_class_oid;
    PGDLLIMPORT Oid binary_upgrade_next_etbl_toast_pg_class_oid;
    PGDLLIMPORT Oid binary_upgrade_next_etbl_heap_pg_class_rfoid;
    PGDLLIMPORT Oid binary_upgrade_next_etbl_index_pg_class_rfoid;
    PGDLLIMPORT Oid binary_upgrade_next_etbl_toast_pg_class_rfoid;

    /* Potentially set by contrib/pg_upgrade_support functions */
    Oid binary_upgrade_next_array_pg_type_oid;
    Oid Inplace_upgrade_next_array_pg_type_oid;

    /* Potentially set by contrib/pg_upgrade_support functions */
    Oid binary_upgrade_next_pg_authid_oid;

    Oid binary_upgrade_next_toast_pg_type_oid;
    int32 binary_upgrade_max_part_toast_pg_type_oid;
    int32 binary_upgrade_cur_part_toast_pg_type_oid;
    Oid* binary_upgrade_next_part_toast_pg_type_oid;
    Oid binary_upgrade_next_heap_pg_class_oid;
    Oid binary_upgrade_next_toast_pg_class_oid;
    Oid binary_upgrade_next_heap_pg_class_rfoid;
    Oid binary_upgrade_next_toast_pg_class_rfoid;
    int32 binary_upgrade_max_part_pg_partition_oid;
    int32 binary_upgrade_cur_part_pg_partition_oid;
    Oid* binary_upgrade_next_part_pg_partition_oid;
    int32 binary_upgrade_max_part_pg_partition_rfoid;
    int32 binary_upgrade_cur_part_pg_partition_rfoid;
    Oid* binary_upgrade_next_part_pg_partition_rfoid;
    int32 binary_upgrade_max_part_toast_pg_class_oid;
    int32 binary_upgrade_cur_part_toast_pg_class_oid;
    Oid* binary_upgrade_next_part_toast_pg_class_oid;
    int32 binary_upgrade_max_part_toast_pg_class_rfoid;
    int32 binary_upgrade_cur_part_toast_pg_class_rfoid;
    Oid* binary_upgrade_next_part_toast_pg_class_rfoid;
    Oid binary_upgrade_next_partrel_pg_partition_oid;
    Oid binary_upgrade_next_partrel_pg_partition_rfoid;
    Oid Inplace_upgrade_next_heap_pg_class_oid;
    Oid Inplace_upgrade_next_toast_pg_class_oid;
    Oid binary_upgrade_next_index_pg_class_oid;
    Oid binary_upgrade_next_index_pg_class_rfoid;
    int32 binary_upgrade_max_part_index_pg_class_oid;
    int32 binary_upgrade_cur_part_index_pg_class_oid;
    Oid* binary_upgrade_next_part_index_pg_class_oid;
    int32 binary_upgrade_max_part_index_pg_class_rfoid;
    int32 binary_upgrade_cur_part_index_pg_class_rfoid;
    Oid* binary_upgrade_next_part_index_pg_class_rfoid;
    int32 bupgrade_max_psort_pg_class_oid;
    int32 bupgrade_cur_psort_pg_class_oid;
    Oid* bupgrade_next_psort_pg_class_oid;
    int32 bupgrade_max_psort_pg_type_oid;
    int32 bupgrade_cur_psort_pg_type_oid;
    Oid* bupgrade_next_psort_pg_type_oid;
    int32 bupgrade_max_psort_array_pg_type_oid;
    int32 bupgrade_cur_psort_array_pg_type_oid;
    Oid* bupgrade_next_psort_array_pg_type_oid;
    int32 bupgrade_max_psort_pg_class_rfoid;
    int32 bupgrade_cur_psort_pg_class_rfoid;
    Oid* bupgrade_next_psort_pg_class_rfoid;
    Oid Inplace_upgrade_next_index_pg_class_oid;
    Oid binary_upgrade_next_pg_enum_oid;
    Oid Inplace_upgrade_next_general_oid;
    int32 bupgrade_max_cudesc_pg_class_oid;
    int32 bupgrade_cur_cudesc_pg_class_oid;
    Oid* bupgrade_next_cudesc_pg_class_oid;
    int32 bupgrade_max_cudesc_pg_type_oid;
    int32 bupgrade_cur_cudesc_pg_type_oid;
    Oid* bupgrade_next_cudesc_pg_type_oid;
    int32 bupgrade_max_cudesc_array_pg_type_oid;
    int32 bupgrade_cur_cudesc_array_pg_type_oid;
    Oid* bupgrade_next_cudesc_array_pg_type_oid;
    int32 bupgrade_max_cudesc_pg_class_rfoid;
    int32 bupgrade_cur_cudesc_pg_class_rfoid;
    Oid* bupgrade_next_cudesc_pg_class_rfoid;
    int32 bupgrade_max_cudesc_index_oid;
    int32 bupgrade_cur_cudesc_index_oid;
    Oid* bupgrade_next_cudesc_index_oid;
    int32 bupgrade_max_cudesc_toast_pg_class_oid;
    int32 bupgrade_cur_cudesc_toast_pg_class_oid;
    Oid* bupgrade_next_cudesc_toast_pg_class_oid;
    int32 bupgrade_max_cudesc_toast_pg_type_oid;
    int32 bupgrade_cur_cudesc_toast_pg_type_oid;
    Oid* bupgrade_next_cudesc_toast_pg_type_oid;
    int32 bupgrade_max_cudesc_toast_index_oid;
    int32 bupgrade_cur_cudesc_toast_index_oid;
    Oid* bupgrade_next_cudesc_toast_index_oid;
    int32 bupgrade_max_cudesc_index_rfoid;
    int32 bupgrade_cur_cudesc_index_rfoid;
    Oid* bupgrade_next_cudesc_index_rfoid;
    int32 bupgrade_max_cudesc_toast_pg_class_rfoid;
    int32 bupgrade_cur_cudesc_toast_pg_class_rfoid;
    Oid* bupgrade_next_cudesc_toast_pg_class_rfoid;
    int32 bupgrade_max_cudesc_toast_index_rfoid;
    int32 bupgrade_cur_cudesc_toast_index_rfoid;
    Oid* bupgrade_next_cudesc_toast_index_rfoid;
    int32 bupgrade_max_delta_toast_pg_class_oid;
    int32 bupgrade_cur_delta_toast_pg_class_oid;
    Oid* bupgrade_next_delta_toast_pg_class_oid;
    int32 bupgrade_max_delta_toast_pg_type_oid;
    int32 bupgrade_cur_delta_toast_pg_type_oid;
    Oid* bupgrade_next_delta_toast_pg_type_oid;
    int32 bupgrade_max_delta_toast_index_oid;
    int32 bupgrade_cur_delta_toast_index_oid;
    Oid* bupgrade_next_delta_toast_index_oid;
    int32 bupgrade_max_delta_toast_pg_class_rfoid;
    int32 bupgrade_cur_delta_toast_pg_class_rfoid;
    Oid* bupgrade_next_delta_toast_pg_class_rfoid;
    int32 bupgrade_max_delta_toast_index_rfoid;
    int32 bupgrade_cur_delta_toast_index_rfoid;
    Oid* bupgrade_next_delta_toast_index_rfoid;
    int32 bupgrade_max_delta_pg_class_oid;
    int32 bupgrade_cur_delta_pg_class_oid;
    Oid* bupgrade_next_delta_pg_class_oid;
    int32 bupgrade_max_delta_pg_type_oid;
    int32 bupgrade_cur_delta_pg_type_oid;
    Oid* bupgrade_next_delta_pg_type_oid;
    int32 bupgrade_max_delta_array_pg_type_oid;
    int32 bupgrade_cur_delta_array_pg_type_oid;
    Oid* bupgrade_next_delta_array_pg_type_oid;
    int32 bupgrade_max_delta_pg_class_rfoid;
    int32 bupgrade_cur_delta_pg_class_rfoid;
    Oid* bupgrade_next_delta_pg_class_rfoid;
    Oid Inplace_upgrade_next_pg_proc_oid;
    Oid Inplace_upgrade_next_pg_namespace_oid;
    Oid binary_upgrade_next_pg_type_oid;
    Oid Inplace_upgrade_next_pg_type_oid;
    bool new_catalog_isshared;
    bool new_catalog_need_storage;
    List* new_shared_catalog_list;
} knl_u_upgrade_context;

typedef struct knl_u_bootstrap_context {
#define MAXATTR 40
#define NAMEDATALEN 64
    int num_columns_read;
    int yyline; /* line number for error reporting */

    struct RelationData* boot_reldesc;                /* current relation descriptor */
    struct FormData_pg_attribute* attrtypes[MAXATTR]; /* points to attribute info */
    int numattr;                                      /* number of attributes for cur. rel */

    struct typmap** Typ;
    struct typmap* Ap;

    MemoryContext nogc; /* special no-gc mem context */
    struct _IndexList* ILHead;
    char newStr[NAMEDATALEN + 1]; /* array type names < NAMEDATALEN long */
} knl_u_bootstrap_context;

typedef enum {
    IDENTIFIER_LOOKUP_NORMAL,  /* normal processing of var names */
    IDENTIFIER_LOOKUP_DECLARE, /* In DECLARE --- don't look up names */
    IDENTIFIER_LOOKUP_EXPR     /* In SQL expression --- special case */
} IdentifierLookup;

typedef struct knl_u_xact_context {
    struct XactCallbackItem* Xact_callbacks;
    struct SubXactCallbackItem* SubXact_callbacks;
#ifdef PGXC
    struct abort_callback_type* dbcleanup_info;
#endif

    /*
     * GID to be used for preparing the current transaction.  This is also
     * global to a whole transaction, so we don't keep it in the state stack.
     */
    char* prepareGID;
    char* savePrepareGID;

    bool pbe_execute_complete;
} knl_u_xact_context;

typedef struct knl_u_plpgsql_context {
    bool inited;
    /* pl_comp.cpp */
    /* A context for func to store in one session. */
    MemoryContext plpgsql_func_cxt;
    /* ----------
     * Hash table for compiled functions
     * ----------
     */
    HTAB* plpgsql_HashTable;
    /* list of functions' cell. */
    DList* plpgsql_dlist_functions;

    int datums_alloc;
    int plpgsql_nDatums;
    int datums_last;
    int plpgsql_IndexErrorVariable;
    char* plpgsql_error_funcname;
    bool plpgsql_DumpExecTree;
    bool plpgsql_check_syntax;
    /* A context appropriate for short-term allocs during compilation */
    MemoryContext compile_tmp_cxt;
    struct PLpgSQL_stmt_block* plpgsql_parse_result;
    struct PLpgSQL_datum** plpgsql_Datums;
    struct PLpgSQL_function* plpgsql_curr_compile;

    /* pl_exec.cpp */
    struct EState* simple_eval_estate;
    struct SimpleEcontextStackEntry* simple_econtext_stack;

    /*
     * "cursor_array" is allocated from top_transaction_mem_cxt, but the variables inside
     * of CursorEntry is created under "CursorEntry" memory context.
     */
    List* cursor_array;

    /* pl_handler.cpp */
    int plpgsql_variable_conflict;
    /* Hook for plugins */
    struct PLpgSQL_plugin** plugin_ptr;

    /* pl_funcs.cpp */
    int dump_indent;

    /* pl_funcs.cpp */
    /* ----------
     * Local variables for namespace handling
     *
     * The namespace structure actually forms a tree, of which only one linear
     * list or "chain" (from the youngest item to the root) is accessible from
     * any one plpgsql statement.  During initial parsing of a function, ns_top
     * points to the youngest item accessible from the block currently being
     * parsed.    We store the entire tree, however, since at runtime we will need
     * to access the chain that's relevant to any one statement.
     *
     * Block boundaries in the namespace chain are marked by PLPGSQL_NSTYPE_LABEL
     * items.
     * ----------
     */
    struct PLpgSQL_nsitem* ns_top;

    /* pl_scanner.cpp */
    /* Klugy flag to tell scanner how to look up identifiers */
    IdentifierLookup plpgsql_IdentifierLookup;
    /*
     * Scanner working state.  At some point we might wish to fold all this
     * into a YY_EXTRA struct.    For the moment, there is no need for plpgsql's
     * lexer to be re-entrant, and the notational burden of passing a yyscanner
     * pointer around is great enough to not want to do it without need.
     */

    /* The stuff the core lexer needs(core_yyscan_t) */
    void* yyscanner;
    struct core_yy_extra_type* core_yy;

    /* The original input string */
    const char* scanorig;

    /* Current token's length (corresponds to plpgsql_yylval and plpgsql_yylloc) */
    int plpgsql_yyleng;

    /* Token pushback stack */
#define MAX_PUSHBACKS 4

    int num_pushbacks;
    int pushback_token[MAX_PUSHBACKS];

    /* State for plpgsql_location_to_lineno() */
    const char* cur_line_start;
    const char* cur_line_end;
    int cur_line_num;
    List* goto_labels;

    struct HTAB* rendezvousHash;
} knl_u_plpgsql_context;

typedef struct knl_u_postgres_context {
    /*
     * Flags to implement skip-till-Sync-after-error behavior for messages of
     * the extended query protocol.
     */
    bool doing_extended_query_message;
    bool ignore_till_sync;
} knl_u_postgres_context;

typedef struct knl_u_stat_context {
    char* pgstat_stat_filename;
    char* pgstat_stat_tmpname;

    struct HTAB* pgStatDBHash;
    struct TabStatusArray* pgStatTabList;

    /*
     * BgWriter global statistics counters (unused in other processes).
     * Stored directly in a stats message structure so it can be sent
     * without needing to copy things around.  We assume this inits to zeroes.
     */
    struct PgStat_MsgBgWriter* BgWriterStats;

    /*
     * Hash table for O(1) t_id -> tsa_entry lookup
     */
    HTAB* pgStatTabHash;

    /* MemoryContext for pgStatTabHash */
    MemoryContext pgStatTabHashContext;

    /*
     * Backends store per-function info that's waiting to be sent to the collector
     * in this hash table (indexed by function OID).
     */
    struct HTAB* pgStatFunctions;

    /*
     * Indicates if backend has some function stats that it hasn't yet
     * sent to the collector.
     */
    bool have_function_stats;

    bool pgStatRunningInCollector;

    struct PgStat_SubXactStatus* pgStatXactStack;

    int pgStatXactCommit;

    int pgStatXactRollback;

    int64 pgStatBlockReadTime;

    int64 pgStatBlockWriteTime;

    struct PgBackendStatus* localBackendStatusTable;

    int localNumBackends;

    /*
     * Cluster wide statistics, kept in the stats collector.
     * Contains statistics that are not collected per database
     * or per table.
     */
    struct PgStat_GlobalStats* globalStats;

    /*
     * Total time charged to functions so far in the current backend.
     * We use this to help separate "self" and "other" time charges.
     * (We assume this initializes to zero.)
     */
    instr_time total_func_time;

    struct HTAB* analyzeCheckHash;

    union NumericValue* osStatDataArray;
    struct OSRunInfoDesc* osStatDescArray;
    TimestampTz last_report;
    bool isTopLevelPlSql;
    int64* localTimeInfoArray;

    MemoryContext pgStatLocalContext;
    MemoryContext pgStatCollectThdStatusContext;

    /* Track memory usage in chunks at individual session level */
    int32 trackedMemChunks;

    /* Track memory usage in bytes at individual session level */
    int64 trackedBytes;
} knl_u_stat_context;

#define MAX_LOCKMETHOD 2

typedef uint16 CycleCtr;
typedef void* Block;
typedef struct knl_u_storage_context {
    /*
     * How many buffers PrefetchBuffer callers should try to stay ahead of their
     * ReadBuffer calls by.  This is maintained by the assign hook for
     * effective_io_concurrency.  Zero means "never prefetch".
     */
    int target_prefetch_pages;

    volatile bool session_timeout_active;
    /* session_fin_time is valid only if session_timeout_active is true */
    TimestampTz session_fin_time;

    /* Number of file descriptors known to be in use by VFD entries. */
    int nfile;

    /*
     * Flag to tell whether it's worth scanning VfdCache looking for temp files
     * to close
     */
    bool have_xact_temporary_files;
    /*
     * Tracks the total size of all temporary files.  Note: when temp_file_limit
     * is being enforced, this cannot overflow since the limit cannot be more
     * than INT_MAX kilobytes.    When not enforcing, it could theoretically
     * overflow, but we don't care.
     */
    uint64 temporary_files_size;
    int numAllocatedDescs;
    int maxAllocatedDescs;
    struct AllocateDesc* allocatedDescs;
    /*
     * Number of temporary files opened during the current session;
     * this is used in generation of tempfile names.
     */
    long tempFileCounter;
    /*
     * Array of OIDs of temp tablespaces.  When numTempTableSpaces is -1,
     * this has not been set in the current transaction.
     */
    Oid* tempTableSpaces;
    int numTempTableSpaces;
    int nextTempTableSpace;
    /* record how many IO request submit in async mode, because if error happen,
     *  we should distinguish which IOs has been submit, if not, the resource clean will
     *  meet error like ref_count, flags
     *  remember set AsyncSubmitIOCount to 0 after resource clean up sucessfully
     */
    int AsyncSubmitIOCount;

    /*
     * Virtual File Descriptor array pointer and size.    This grows as
     * needed.    'File' values are indexes into this array.
     * Note that VfdCache[0] is not a usable VFD, just a list header.
     */
    struct vfd* VfdCache;
    Size SizeVfdCache;

    /*
     * Each backend has a hashtable that stores all extant SMgrRelation objects.
     * In addition, "unowned" SMgrRelation objects are chained together in a list.
     */
    struct HTAB* SMgrRelationHash;
    dlist_head unowned_reln;

    MemoryContext MdCxt; /* context for all md.c allocations */
    struct HTAB* pendingOpsTable;
    List* pendingUnlinks;
    CycleCtr mdsync_cycle_ctr;
    CycleCtr mdckpt_cycle_ctr;
    bool mdsync_in_progress;

    LocalTransactionId nextLocalTransactionId;

    /*
     * Whether current session is holding a session lock after transaction commit.
     * This may appear ugly, but since at present all lock information is stored in
     * per-thread pgproc, we should not decouple session from thread if session lock
     * is held.
     * We now set this flag at LockReleaseAll phase during transaction commit or abort.
     * As a result, all callbackups for releasing session locks should come before it.
     */
    bool holdSessionLock[MAX_LOCKMETHOD];

    /* true if datanode is within the process of two phase commit */
    bool twoPhaseCommitInProgress;
    int32 dumpHashbucketIdNum;
    int2 *dumpHashbucketIds;

    /* Pointers to shared state */
    // struct BufferStrategyControl* StrategyControl;
    int NLocBuffer; /* until buffers are initialized */
    struct BufferDesc* LocalBufferDescriptors;
    Block* LocalBufferBlockPointers;
    int32* LocalRefCount;
    int nextFreeLocalBuf;
    struct HTAB* LocalBufHash;
    char* cur_block;
    int next_buf_in_block;
    int num_bufs_in_block;
    int total_bufs_allocated;
    MemoryContext LocalBufferContext;
} knl_u_storage_context;


typedef struct knl_u_libpq_context {
    /*
     * LO "FD"s are indexes into the cookies array.
     *
     * A non-null entry is a pointer to a LargeObjectDesc allocated in the
     * LO private memory context "fscxt".  The cookies array itself is also
     * dynamically allocated in that context.  Its current allocated size is
     * cookies_len entries, of which any unused entries will be NULL.
     */
    struct LargeObjectDesc** cookies;
    int cookies_size;
    MemoryContext fscxt;
    GS_UCHAR* server_key;

    /*
     * These variables hold the pre-parsed contents of the ident usermap
     * configuration file.    ident_lines is a triple-nested list of lines, fields
     * and tokens, as returned by tokenize_file.  There will be one line in
     * ident_lines for each (non-empty, non-comment) line of the file.    Note there
     * will always be at least one field, since blank lines are not entered in the
     * data structure.    ident_line_nums is an integer list containing the actual
     * line number for each line represented in ident_lines.  ident_context is
     * the memory context holding all this.
     */
    List* ident_lines;
    List* ident_line_nums;
    MemoryContext ident_context;
    bool IsConnFromCmAgent;
#ifdef USE_SSL
    bool ssl_loaded_verify_locations;
    SSL_CTX* SSL_server_context;
#endif
} knl_u_libpq_context;

typedef struct knl_u_relcache_context {
    struct HTAB* RelationIdCache;

    /*
     * This flag is false until we have prepared the critical relcache entries
     * that are needed to do indexscans on the tables read by relcache building.
     * Should be used only by relcache.c and catcache.c
     */
    bool criticalRelcachesBuilt;

    /*
     * This flag is false until we have prepared the critical relcache entries
     * for shared catalogs (which are the tables needed for login).
     * Should be used only by relcache.c and postinit.c
     */
    bool criticalSharedRelcachesBuilt;

    /*
     * This counter counts relcache inval events received since backend startup
     * (but only for rels that are actually in cache).    Presently, we use it only
     * to detect whether data about to be written by write_relcache_init_file()
     * might already be obsolete.
     */
    long relcacheInvalsReceived;

    /*
     * This list remembers the OIDs of the non-shared relations cached in the
     * database's local relcache init file.  Note that there is no corresponding
     * list for the shared relcache init file, for reasons explained in the
     * comments for RelationCacheInitFileRemove.
     */
    List* initFileRelationIds;

    /*
     * This flag lets us optimize away work in AtEO(Sub)Xact_RelationCache().
     */
    bool need_eoxact_work;

    HTAB* OpClassCache;

    struct tupleDesc* pgclassdesc;

    struct tupleDesc* pgindexdesc;

    /*
     * BucketMap Cache, consists of a list of BucketMapCache element.
     * Location information of every rel cache is actually pointed to these list
     * members.
     * Attention: we need to invalidate bucket map caches when accepting
     * SI messages of tuples in PGXC_GROUP or SI reset messages!
     */
    List* g_bucketmap_cache;
    uint32 max_bucket_map_size;
} knl_u_relcache_context;

#if defined(HAVE_SETPROCTITLE)
#define PS_USE_SETPROCTITLE
#elif defined(HAVE_PSTAT) && defined(PSTAT_SETCMD)
#define PS_USE_PSTAT
#elif defined(HAVE_PS_STRINGS)
#define PS_USE_PS_STRINGS
#elif (defined(BSD) || defined(__hurd__)) && !defined(__darwin__)
#define PS_USE_CHANGE_ARGV
#elif defined(__linux__) || defined(_AIX) || defined(__sgi) || (defined(sun) && !defined(BSD)) || defined(ultrix) || \
    defined(__ksr__) || defined(__osf__) || defined(__svr5__) || defined(__darwin__)
#define PS_USE_CLOBBER_ARGV
#elif defined(WIN32)
#define PS_USE_WIN32
#else
#define PS_USE_NONE
#endif

typedef struct knl_u_ps_context {
#ifndef PS_USE_CLOBBER_ARGV
/* all but one option need a buffer to write their ps line in */
#define PS_BUFFER_SIZE 256
    char ps_buffer[PS_BUFFER_SIZE];
    size_t ps_buffer_size;
#else                            /* PS_USE_CLOBBER_ARGV */
    char* ps_buffer;        /* will point to argv area */
    size_t ps_buffer_size;  /* space determined at run time */
    size_t last_status_len; /* use to minimize length of clobber */
#endif                           /* PS_USE_CLOBBER_ARGV */
    size_t ps_buffer_cur_len;    /* nominal strlen(ps_buffer) */
    size_t ps_buffer_fixed_size; /* size of the constant prefix */

    /* save the original argv[] location here */
    int save_argc;
    char** save_argv;
} knl_u_ps_context;

typedef struct ParctlState {
    unsigned char global_reserve;    /* global reserve active statement flag */
    unsigned char rp_reserve;        /* resource pool reserve active statement flag */
    unsigned char reserve;           /* reserve active statement flag */
    unsigned char release;           /* release active statement flag */
    unsigned char global_release;    /* global release active statement */
    unsigned char rp_release;        /* resource pool release active statement */
    unsigned char except;            /* a flag to handle exception while the query executing */
    unsigned char special;           /* check the query whether is a special query */
    unsigned char transact;          /* check the query whether is in a transaction block */
    unsigned char transact_begin;    /* check the query if "begin transaction" */
    unsigned char simple;            /* check the query whether is a simple query */
    unsigned char iocomplex;         /* check the query whether is a IO simple query */
    unsigned char enqueue;           /* check the query whether do global parallel control */
    unsigned char errjmp;            /* this is error jump point */
    unsigned char global_waiting;    /* waiting in the global list */
    unsigned char respool_waiting;   /* waiting in the respool list */
    unsigned char preglobal_waiting; /* waiting in simple global list */
    unsigned char central_waiting;   /* waiting in central_waiting */
    unsigned char attach;            /* attach cgroup */
    unsigned char subquery;          /* check the query whether is in a stored procedure */
} ParctlState;

typedef struct knl_u_relmap_context {
    /*
     * The currently known contents of the shared map file and our database's
     * local map file are stored here.    These can be reloaded from disk
     * immediately whenever we receive an update sinval message.
     */
    struct RelMapFile* shared_map;
    struct RelMapFile* local_map;

    /*
     * We use the same RelMapFile data structure to track uncommitted local
     * changes in the mappings (but note the magic and crc fields are not made
     * valid in these variables).  Currently, map updates are not allowed within
     * subtransactions, so one set of transaction-level changes is sufficient.
     *
     * The active_xxx variables contain updates that are valid in our transaction
     * and should be honored by RelationMapOidToFilenode.  The pending_xxx
     * variables contain updates we have been told about that aren't active yet;
     * they will become active at the next CommandCounterIncrement.  This setup
     * lets map updates act similarly to updates of pg_class rows, ie, they
     * become visible only at the next CommandCounterIncrement boundary.
     */
    struct RelMapFile* active_shared_updates;
    struct RelMapFile* active_local_updates;
    struct RelMapFile* pending_shared_updates;
    struct RelMapFile* pending_local_updates;

    /* Hash table for informations about each relfilenode <-> oid pair */
    struct HTAB* RelfilenodeMapHash;
} knl_u_relmap_context;

typedef struct knl_u_unique_sql_context {
    /* each sql should have one unique query id
     * limitation: Query* is needed by calculating unique query id,
     * and Query * is generated after parsing and
     * rewriting SQL, so unique query id only can be used
     * after SQL rewrite
     */
    uint64 unique_sql_id;
    Oid unique_sql_user_id;
    uint32 unique_sql_cn_id;

    /*
     * storing unique sql start time,
     * in PortalRun method, we will update unique sql elapse time,
     * use it to store the start time.
     *
     * Note: as in exec_simple_query, we can get multi parsetree,
     * and will generate multi the unique sql id, so we set
     * unique_sql_start_time at each LOOP(parsetree) started
     *
     * exec_simple_query
     *
     *	pg_parse_query(parsetree List)
     *
     *	LOOP start
     *	****-> set unique_sql_start_time
     *	pg_analyze_and_rewrite
     *		analyze
     *		rewrite
     *	pg_plan_queries
     *	PortalStart
     *	PortalRun
     *		****-> UpdateUniqueSQLStat/UpdateUniqueSQLElapseTime
     *	PortalDrop
     *
     *	LOOP end
     */
    int64 unique_sql_start_time;

    /* unique sql's returned rows counter, only updated on CN */
    uint64 unique_sql_returned_rows_counter;

    /* parse counter, both CN and DN can update the counter  */
    uint64 unique_sql_soft_parse;
    uint64 unique_sql_hard_parse;

    /*
     * last_stat_counter - store pgStatTabList's total counter values when
     * 			exit from pgstat_report_stat last time
     * current_table_counter - store current unique sql id's row activity stat
     */
    struct PgStat_TableCounts* last_stat_counter;
    struct PgStat_TableCounts* current_table_counter;

    /*
     * handle multi SQLs case in exec_simple_query function,
     * for below case:
     *   - we send sqls "select * from xx; update xx;" using libpq
     *     PQexec method(refer to gs_ctl tool)
     *   - then exec_simple_query will get two SQLs
     *   - pg_parse_query will generate parsetree list with two nodes
     *   - then run twice:
     *      # call unique_sql_post_parse_analyze(generate unique sql id/
     *        normalized sql string)
     *        -- here we get the sql using debug_query_string or ParseState.p_sourcetext
     *        -- but the two way to get sql string will be two SQLs, not single one
     *        -- so we need add below two variables to handle this case
     *   	# call PortalDefineQuery
     *   	# call PortalRun -> update unique sql stat
     */
    bool is_multi_unique_sql;
    char* curr_single_unique_sql;

    /*
     * for case unique sql track type is 'UNIQUE_SQL_TRACK_TOP'
     * main logic(only happened on CN):
     * - postgresmain
     *   >> is_top_unique_sql = false
     *
     * - sql parse -> generate unique sql id(top SQL)
     *   >> is_top_unique_sql = true
     *
     * - exec_simple_query
     *   or exec_bind_message
     *   >> PortalRun -> using TOP SQL's unique sql id(will sned to DN)
     *
     * - is_top_unique_sql to false
     */
    bool is_top_unique_sql;
} knl_u_unique_sql_context;

typedef struct knl_u_percentile_context {
    struct SqlRTInfo* LocalsqlRT;
    int LocalCounter;
} knl_u_percentile_context;

typedef struct knl_u_user_login_context {
    /*
     * when user is login, will update the variable in PerformAuthentication,
     * and update the user's login counter.
     *
     * when proc exit, we register the callback function, in the callback
     * function, will update the logout counter, and reset CurrentInstrLoginUserOid.
     */
    Oid CurrentInstrLoginUserOid;
} knl_u_user_login_context;

#define MAXINVALMSGS 32
typedef struct knl_u_inval_context {
    int32 deepthInAcceptInvalidationMessage;

    struct TransInvalidationInfo* transInvalInfo;

    union SharedInvalidationMessage* SharedInvalidMessagesArray;

    int numSharedInvalidMessagesArray;

    int maxSharedInvalidMessagesArray;

    struct SYSCACHECALLBACK* syscache_callback_list;

    int syscache_callback_count;

    struct RELCACHECALLBACK* relcache_callback_list;

    int relcache_callback_count;

    struct PARTCACHECALLBACK* partcache_callback_list;

    int partcache_callback_count;

    uint64 SharedInvalidMessageCounter;

    volatile sig_atomic_t catchupInterruptPending;

    union SharedInvalidationMessage* messages;

    volatile int nextmsg;

    volatile int nummsgs;
} knl_u_inval_context;

typedef struct knl_u_cache_context {
    /* num of cached re's(regular expression) */
    int num_res;

    /* cached re's (regular expression) */
    struct cached_re_str* re_array;

    /*
     * We frequently need to test whether a given role is a member of some other
     * role.  In most of these tests the "given role" is the same, namely the
     * active current user.  So we can optimize it by keeping a cached list of
     * all the roles the "given role" is a member of, directly or indirectly.
     * The cache is flushed whenever we detect a change in pg_auth_members.
     *
     * There are actually two caches, one computed under "has_privs" rules
     * (do not recurse where rolinherit isn't true) and one computed under
     * "is_member" rules (recurse regardless of rolinherit).
     *
     * Possibly this mechanism should be generalized to allow caching membership
     * info for multiple roles?
     *
     * The has_privs cache is:
     * cached_privs_role is the role OID the cache is for.
     * cached_privs_roles is an OID list of roles that cached_privs_role
     *        has the privileges of (always including itself).
     * The cache is valid if cached_privs_role is not InvalidOid.
     *
     * The is_member cache is similarly:
     * cached_member_role is the role OID the cache is for.
     * cached_membership_roles is an OID list of roles that cached_member_role
     *        is a member of (always including itself).
     * The cache is valid if cached_member_role is not InvalidOid.
     */
    Oid cached_privs_role;

    List* cached_privs_roles;

    Oid cached_member_role;

    List* cached_membership_roles;

    struct _SPI_plan* plan_getrulebyoid;

    struct _SPI_plan* plan_getviewrule;

    /* Hash table for informations about each attribute's options */
    struct HTAB* att_opt_cache_hash;

    /* Cache management header --- pointer is NULL until created */
    struct CatCacheHeader* cache_header;

    /* Hash table for information about each tablespace */
    struct HTAB* TableSpaceCacheHash;

    struct HTAB* PartitionIdCache;

    struct HTAB* BucketIdCache;

    bool part_cache_need_eoxact_work;

    bool bucket_cache_need_eoxact_work;	
} knl_u_cache_context;


typedef struct knl_u_syscache_context {
    struct CatCache** SysCache;

    bool CacheInitialized;

    Oid* SysCacheRelationOid;
} knl_u_syscache_context;

namespace dfs {
class DFSConnector;
}
typedef struct knl_u_catalog_context {
    bool nulls[4];
    struct PartitionIdentifier* route;

    /*
     * If "Create function ... LANGUAGE SQL" include agg function, agg->aggtype
     * is the final aggtype. While for "Select agg()", agg->aggtype should be agg->aggtrantype.
     * Here we use Parse_sql_language to distinguish these two cases.
     */
    bool Parse_sql_language;
    struct PendingRelDelete* pendingDeletes; /* head of linked list */
    /* Handle deleting BCM files.
     *
     * For column relation, one bcm file for each column file.
     * For row relation, only one bcm file for the whole relation.
     * We have to handle BCM files of some column file during rollback.
     * Take an example, ADD COLUMN or SET TABLESPACE, may be canceled by user in some cases.
     * Shared buffer must be invalided before BCM files are deleted.
     * See also CStoreCopyColumnDataEnd(), RelationDropStorage(), ect.
     */
    struct RelFileNodeBackend* ColMainFileNodes;
    int ColMainFileNodesMaxNum;
    int ColMainFileNodesCurNum;
    List* pendingDfsDeletes;
    dfs::DFSConnector* delete_conn;
    struct StringInfoData* vf_store_root;
    Oid currentlyReindexedHeap;
    Oid currentlyReindexedIndex;
    List* pendingReindexedIndexes;
    /* These variables define the actually active state: */
    List* activeSearchPath;
    /* default place to create stuff; if InvalidOid, no default */
    Oid activeCreationNamespace;
    /* if TRUE, activeCreationNamespace is wrong, it should be temp namespace */
    bool activeTempCreationPending;
    /* These variables are the values last derived from namespace_search_path: */
    List* baseSearchPath;
    Oid baseCreationNamespace;
    bool baseTempCreationPending;
    Oid namespaceUser;
    /* The above four values are valid only if baseSearchPathValid */
    bool baseSearchPathValid;
    List* overrideStack;
    bool overrideStackValid;
    Oid myTempNamespaceOld;
    /*
     * myTempNamespace is InvalidOid until and unless a TEMP namespace is set up
     * in a particular backend session (this happens when a CREATE TEMP TABLE
     * command is first executed).    Thereafter it's the OID of the temp namespace.
     *
     * myTempToastNamespace is the OID of the namespace for my temp tables' toast
     * tables.    It is set when myTempNamespace is, and is InvalidOid before that.
     *
     * myTempNamespaceSubID shows whether we've created the TEMP namespace in the
     * current subtransaction.    The flag propagates up the subtransaction tree,
     * so the main transaction will correctly recognize the flag if all
     * intermediate subtransactions commit.  When it is InvalidSubTransactionId,
     * we either haven't made the TEMP namespace yet, or have successfully
     * committed its creation, depending on whether myTempNamespace is valid.
     */
    Oid myTempNamespace;
    Oid myTempToastNamespace;
    bool deleteTempOnQuiting;
    SubTransactionId myTempNamespaceSubID;
    /* stuff for online expansion redis-cancel */
    bool redistribution_cancelable;
} knl_u_catalog_context;

typedef struct knl_u_pgxc_context {
    /* Current size of dn_handles and co_handles */
    int NumDataNodes;
    int NumCoords;
    int NumStandbyDataNodes;

    /* Number of connections held */
    int datanode_count;
    int coord_count;

    /* dn oid matrics for multiple standby deployment */
    Oid** dn_matrics;

    /*
     * Datanode handles saved in session memory context
     * when PostgresMain is launched.
     * Those handles are used inside a transaction by Coordinator to Datanodes.
     */
    struct pgxc_node_handle* dn_handles;
    /*
     * Coordinator handles saved in session memory context
     * when PostgresMain is launched.
     * Those handles are used inside a transaction by Coordinator to Coordinators
     */
    struct pgxc_node_handle* co_handles;

    struct RemoteXactState* remoteXactState;

    int PGXCNodeId;
    /*
     * When a particular node starts up, store the node identifier in this variable
     * so that we dont have to calculate it OR do a search in cache any where else
     * This will have minimal impact on performance
     */
    uint32 PGXCNodeIdentifier;

    /*
     * List of PGXCNodeHandle to track readers and writers involved in the
     * current transaction
     */
    List* XactWriteNodes;
    List* XactReadNodes;
    char* preparedNodes;

    /* Pool */
    char sock_path[MAXPGPATH];
    int last_reported_send_errno;
    bool PoolerResendParams;
    struct PGXCNodeConnectionInfo* PoolerConnectionInfo;
    struct PoolAgent* poolHandle;

    List* connection_cache;
    List* connection_cache_handle;

    bool is_gc_fdw;
    bool is_gc_fdw_analyze;
    int gc_fdw_current_idx;
    int gc_fdw_max_idx;
    int gc_fdw_run_version;
    struct SnapshotData* gc_fdw_snapshot;
} knl_u_pgxc_context;

typedef struct knl_u_fmgr_context {
    struct df_files_init* file_list_init;

    struct df_files_init* file_init_tail;
} knl_u_fmgr_context;

typedef struct knl_u_erand_context {
    unsigned short rand48_seed[3];
} knl_u_erand_context;

typedef struct knl_u_regex_context {
    int pg_regex_strategy; /* enum PG_Locale_Strategy */
    Oid pg_regex_collation;
    struct pg_ctype_cache* pg_ctype_cache_list;
} knl_u_regex_context;

namespace MOT {
  class SessionContext;
  class TxnManager;
}

namespace JitExec {
    struct JitContext;
    struct JitContextPool;
}

namespace tvm {
    class JitIf;
    class JitWhile;
    class JitDoWhile;
}

namespace llvm {
    class JitIf;
    class JitWhile;
    class JitDoWhile;
}

typedef struct knl_u_mot_context {
    bool callbacks_set;

    // session attributes
    uint32_t session_id;
    uint32_t connection_id;
    MOT::SessionContext* session_context;
    MOT::TxnManager* txn_manager;

    // JIT
    JitExec::JitContextPool* jit_session_context_pool;
    uint32_t jit_context_count;
    llvm::JitIf* jit_llvm_if_stack;
    llvm::JitWhile* jit_llvm_while_stack;
    llvm::JitDoWhile* jit_llvm_do_while_stack;
    tvm::JitIf* jit_tvm_if_stack;
    tvm::JitWhile* jit_tvm_while_stack;
    tvm::JitDoWhile* jit_tvm_do_while_stack;
    JitExec::JitContext* jit_context;
    MOT::TxnManager* jit_txn;
} knl_u_mot_context;

typedef struct knl_u_gtt_context {
    bool gtt_cleaner_exit_registered;
    HTAB* gtt_storage_local_hash;
    MemoryContext gtt_relstats_context;

    /* relfrozenxid of all gtts in the current session */
    List* gtt_session_relfrozenxid_list;
    TransactionId gtt_session_frozenxid;
    pg_on_exit_callback gtt_sess_exit;
} knl_u_gtt_context;

enum knl_ext_fdw_type {
    MYSQL_TYPE_FDW,
    ORACLE_TYPE_FDW,
    POSTGRES_TYPE_FDW,
    PLDEBUG_TYPE,
    /* Add new external FDW type before MAX_TYPE_FDW */
    MAX_TYPE_FDW
};

typedef struct knl_u_ext_fdw_context {
    union {
        void* connList;                     /* Connection info to other DB */
        void* pldbg_ctx;                    /* Pldebugger info */
    };
    pg_on_exit_callback fdwExitFunc;    /* Exit callback, will be called when session exit */
} knl_u_ext_fdw_context;

/* Info need to pass from leader to worker */
struct ParallelHeapScanDescData;
typedef uint64 XLogRecPtr;
typedef struct ParallelQueryInfo {
    struct SharedExecutorInstrumentation *instrumentation;
    BufferUsage *bufUsage;
    char *tupleQueue;
    char *pstmt_space;
    char *param_space;
    Size param_len;
    int pscan_num;
    ParallelHeapScanDescData **pscan;
} ParallelQueryInfo;

struct BTShared;
struct SharedSort;
typedef struct ParallelBtreeInfo {
    char *queryText;
    BTShared *btShared;
    SharedSort *sharedSort;
    SharedSort *sharedSort2;
} ParallelBtreeInfo;

typedef struct ParallelInfoContext {
    Oid database_id;
    Oid authenticated_user_id;
    Oid current_user_id;
    Oid outer_user_id;
    Oid temp_namespace_id;
    Oid temp_toast_namespace_id;
    int sec_context;
    bool is_superuser;
    void *parallel_master_pgproc; /* PGPROC */
    ThreadId parallel_master_pid;
    BackendId parallel_master_backend_id;
    TimestampTz xact_ts;
    TimestampTz stmt_ts;
    int usedComboCids;
    struct ComboCidKeyData *comboCids;
    char *tsnapspace;
    Size tsnapspace_len;
    char *asnapspace;
    Size asnapspace_len;
    struct RelMapFile *active_shared_updates;
    struct RelMapFile *active_local_updates;
    char *errorQueue;
    int xactIsoLevel;
    bool xactDeferrable;
    TransactionId topTransactionId;
    TransactionId currentTransactionId;
    TransactionId RecentGlobalXmin;
    TransactionId TransactionXmin;
    TransactionId RecentXmin;
    /* CurrentSnapshot */
    TransactionId xmin;
    TransactionId xmax;
    CommandId curcid;
    uint32 timeline;
    CommitSeqNo snapshotcsn;
    CommandId currentCommandId;
    int nParallelCurrentXids;
    TransactionId *ParallelCurrentXids;
    char *library_name;
    char *function_name;
    char *namespace_search_path;
#ifdef __USE_NUMA
    int numaNode;
    cpu_set_t *cpuset;
#endif

    union {
        ParallelQueryInfo queryInfo; /* parameters for parallel query only */
        ParallelBtreeInfo btreeInfo; /* parameters for parallel create index(btree) only */
    };

    /* Mutex protects remaining fields. */
    slock_t mutex;
    /* Maximum XactLastRecEnd of any worker. */
    XLogRecPtr last_xlog_end;
} ParallelInfoContext;

typedef struct knl_u_parallel_context {
    ParallelInfoContext *pwCtx;
    MemoryContext memCtx; /* memory context used to malloc memory */
    slist_head on_detach; /* On-detach callbacks. */
    bool used; /* used or not */
} knl_u_parallel_context;

enum knl_session_status {
    KNL_SESS_FAKE,
    KNL_SESS_UNINIT,
    KNL_SESS_ATTACH,
    KNL_SESS_DETACH,
    KNL_SESS_CLOSE,
    KNL_SESS_END_PHASE1,
    KNL_SESS_CLOSERAW,  // not initialize and
};

typedef struct knl_session_context {
    volatile knl_session_status status;
    Dlelem elem;

    ThreadId attachPid;

    MemoryContext top_mem_cxt;
    MemoryContext cache_mem_cxt;
    MemoryContext top_transaction_mem_cxt;
    MemoryContext self_mem_cxt;
    MemoryContext top_portal_cxt;
    /* temp_mem_cxt is a context which will be reset when the session attach to a thread */
    MemoryContext temp_mem_cxt;
    int session_ctr_index;
    uint64 session_id;
    uint64 debug_query_id;
    uint64 global_sess_id;
    long cancel_key;
    char* prog_name;

    bool ClientAuthInProgress;

    bool need_report_top_xid;

    struct config_generic** guc_variables;

    int num_guc_variables;

    int on_sess_exit_index;

    knl_session_attr attr;
    struct knl_u_wlm_context* wlm_cxt;
    knl_u_analyze_context analyze_cxt;
    knl_u_cache_context cache_cxt;
    knl_u_catalog_context catalog_cxt;
    knl_u_commands_context cmd_cxt;
    knl_u_contrib_context contrib_cxt;
    knl_u_erand_context rand_cxt;
    knl_u_executor_context exec_cxt;
    knl_u_fmgr_context fmgr_cxt;
    knl_u_index_context index_cxt;
    knl_u_instrument_context instr_cxt;
    knl_u_inval_context inval_cxt;
    knl_u_locale_context lc_cxt;
    knl_u_log_context log_cxt;
    knl_u_libpq_context libpq_cxt;
    knl_u_mb_context mb_cxt;
    knl_u_misc_context misc_cxt;
    knl_u_optimizer_context opt_cxt;
    knl_u_parser_context parser_cxt;
    knl_u_pgxc_context pgxc_cxt;
    knl_u_plancache_context pcache_cxt;
    knl_u_plpgsql_context plsql_cxt;
    knl_u_postgres_context postgres_cxt;
    knl_u_proc_context proc_cxt;
    knl_u_ps_context ps_cxt;
    knl_u_regex_context regex_cxt;
    knl_u_xact_context xact_cxt;
    knl_u_sig_context sig_cxt;
    knl_u_SPI_context SPI_cxt;
    knl_u_relcache_context relcache_cxt;
    knl_u_relmap_context relmap_cxt;
    knl_u_stat_context stat_cxt;
    knl_u_storage_context storage_cxt;
    knl_u_stream_context stream_cxt;
    knl_u_syscache_context syscache_cxt;
    knl_u_time_context time_cxt;
    knl_u_trigger_context tri_cxt;
    knl_u_tscache_context tscache_cxt;
    knl_u_typecache_context tycache_cxt;
    knl_u_upgrade_context upg_cxt;
    knl_u_utils_context utils_cxt;
    knl_u_mot_context mot_cxt;

    /* instrumentation */
    knl_u_unique_sql_context unique_sql_cxt;
    knl_u_user_login_context user_login_cxt;
    knl_u_percentile_context percentile_cxt;

    /* GTT */
    knl_u_gtt_context gtt_ctx;

    /* external FDW */
    knl_u_ext_fdw_context ext_fdw_ctx[MAX_TYPE_FDW];

    /* parallel query context */
    knl_u_parallel_context parallel_ctx[DSM_MAX_ITEM_PER_QUERY];
} knl_session_context;

extern knl_session_context* create_session_context(MemoryContext parent, uint64 id);

extern void knl_session_init(knl_session_context* sess_cxt);

extern THR_LOCAL knl_session_context* u_sess;

#endif /* SRC_INCLUDE_KNL_KNL_SESSION_H_ */
