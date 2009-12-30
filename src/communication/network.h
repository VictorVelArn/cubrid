/*
 * Copyright (C) 2008 Search Solution Corporation. All rights reserved by Search Solution.
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */


/*
 * network.h -Definitions for client/server network support.
 */

#ifndef _NETWORK_H_
#define _NETWORK_H_

#ident "$Id$"

#include "query_opfunc.h"
#include "perf_monitor.h"
#include "locator.h"
#include "log_comm.h"
#include "thread_impl.h"


/* Server statistics structure size, used to make sure the pack/unpack
   routines follow the current structure definition.
   This must be the byte size of the structure
   as returned by sizeof().  Note that MEMORY_STAT_SIZE and PACKED_STAT_SIZE
   are not necesarily the same although they will be in most cases.
*/
#define STAT_SIZE_PACKED (OR_INT_SIZE * 32)
#define STAT_SIZE_MEMORY STAT_SIZE_PACKED

#define GLOBAL_STAT_SIZE_PACKED (OR_INT64_SIZE * 32)

/* These define the requests that the server will respond to */
enum net_server_request
{
  NET_SERVER_REQUEST_START = 0,

  NET_SERVER_PING,

  NET_SERVER_BO_INIT_SERVER,
  NET_SERVER_BO_REGISTER_CLIENT,
  NET_SERVER_BO_UNREGISTER_CLIENT,
  NET_SERVER_BO_BACKUP,
  NET_SERVER_BO_ADD_VOLEXT,
  NET_SERVER_BO_CHECK_DBCONSISTENCY,
  NET_SERVER_BO_FIND_NPERM_VOLS,
  NET_SERVER_BO_FIND_NTEMP_VOLS,
  NET_SERVER_BO_FIND_LAST_TEMP,
  NET_SERVER_BO_CHANGE_HA_MODE,
  NET_SERVER_BO_NOTIFY_HA_LOG_APPLIER_STATE,

  NET_SERVER_TM_SERVER_COMMIT,
  NET_SERVER_TM_SERVER_ABORT,
  NET_SERVER_TM_SERVER_START_TOPOP,
  NET_SERVER_TM_SERVER_END_TOPOP,
  NET_SERVER_TM_SERVER_SAVEPOINT,
  NET_SERVER_TM_SERVER_PARTIAL_ABORT,
  NET_SERVER_TM_SERVER_HAS_UPDATED,
  NET_SERVER_TM_SERVER_ISACTIVE_AND_HAS_UPDATED,
  NET_SERVER_TM_ISBLOCKED,
  NET_SERVER_TM_WAIT_SERVER_ACTIVE_TRANS,
  NET_SERVER_TM_SERVER_GET_GTRINFO,
  NET_SERVER_TM_SERVER_SET_GTRINFO,
  NET_SERVER_TM_SERVER_2PC_START,
  NET_SERVER_TM_SERVER_2PC_PREPARE,
  NET_SERVER_TM_SERVER_2PC_RECOVERY_PREPARED,
  NET_SERVER_TM_SERVER_2PC_ATTACH_GT,
  NET_SERVER_TM_SERVER_2PC_PREPARE_GT,
  NET_SERVER_TM_LOCAL_TRANSACTION_ID,

  NET_SERVER_LC_FETCH,
  NET_SERVER_LC_FETCHALL,
  NET_SERVER_LC_FETCH_LOCKSET,
  NET_SERVER_LC_FETCH_ALLREFS_LOCKSET,
  NET_SERVER_LC_GET_CLASS,
  NET_SERVER_LC_FIND_CLASSOID,
  NET_SERVER_LC_DOESEXIST,
  NET_SERVER_LC_FORCE,
  NET_SERVER_LC_RESERVE_CLASSNAME,
  NET_SERVER_LC_DELETE_CLASSNAME,
  NET_SERVER_LC_RENAME_CLASSNAME,
  NET_SERVER_LC_ASSIGN_OID,
  NET_SERVER_LC_NOTIFY_ISOLATION_INCONS,
  NET_SERVER_LC_FIND_LOCKHINT_CLASSOIDS,
  NET_SERVER_LC_FETCH_LOCKHINT_CLASSES,
  NET_SERVER_LC_ASSIGN_OID_BATCH,
  NET_SERVER_LC_BUILD_FK_OBJECT_CACHE,
  NET_SERVER_LC_REM_CLASS_FROM_INDEX,

  NET_SERVER_HEAP_CREATE,
  NET_SERVER_HEAP_DESTROY,
  NET_SERVER_HEAP_DESTROY_WHEN_NEW,
  NET_SERVER_HEAP_GET_CLASS_NOBJS_AND_NPAGES,
  NET_SERVER_HEAP_HAS_INSTANCE,

  NET_SERVER_LARGEOBJMGR_CREATE,
  NET_SERVER_LARGEOBJMGR_READ,
  NET_SERVER_LARGEOBJMGR_WRITE,
  NET_SERVER_LARGEOBJMGR_INSERT,
  NET_SERVER_LARGEOBJMGR_DESTROY,
  NET_SERVER_LARGEOBJMGR_DELETE,
  NET_SERVER_LARGEOBJMGR_APPEND,
  NET_SERVER_LARGEOBJMGR_TRUNCATE,
  NET_SERVER_LARGEOBJMGR_COMPRESS,
  NET_SERVER_LARGEOBJMGR_LENGTH,

  NET_SERVER_LOG_RESET_WAITSECS,
  NET_SERVER_LOG_RESET_ISOLATION,
  NET_SERVER_LOG_SET_INTERRUPT,
  NET_SERVER_LOG_CLIENT_UNDO,
  NET_SERVER_LOG_CLIENT_POSTPONE,
  NET_SERVER_LOG_HAS_FINISHED_CLIENT_POSTPONE,
  NET_SERVER_LOG_HAS_FINISHED_CLIENT_UNDO,
  NET_SERVER_LOG_CLIENT_GET_FIRST_POSTPONE,
  NET_SERVER_LOG_CLIENT_GET_FIRST_UNDO,
  NET_SERVER_LOG_CLIENT_GET_NEXT_POSTPONE,
  NET_SERVER_LOG_CLIENT_GET_NEXT_UNDO,
  NET_SERVER_LOG_CLIENT_UNKNOWN_STATE_ABORT_GET_FIRST_UNDO,
  NET_SERVER_LOG_DUMP_STAT,
  NET_SERVER_LOG_GETPACK_TRANTB,
  NET_SERVER_LOG_DUMP_TRANTB,

  NET_SERVER_LK_DUMP,

  NET_SERVER_BTREE_ADDINDEX,
  NET_SERVER_BTREE_DELINDEX,
  NET_SERVER_BTREE_LOADINDEX,
  NET_SERVER_BTREE_FIND_UNIQUE,
  NET_SERVER_BTREE_CLASS_UNIQUE_TEST,
  NET_SERVER_BTREE_GET_STATISTICS,

  NET_SERVER_DISK_TOTALPGS,
  NET_SERVER_DISK_FREEPGS,
  NET_SERVER_DISK_REMARKS,
  NET_SERVER_DISK_PURPOSE,
  NET_SERVER_DISK_PURPOSE_TOTALPGS_AND_FREEPGS,
  NET_SERVER_DISK_VLABEL,

  NET_SERVER_QST_SERVER_GET_STATISTICS,
  NET_SERVER_QST_UPDATE_CLASS_STATISTICS,
  NET_SERVER_QST_UPDATE_STATISTICS,

  NET_SERVER_QM_QUERY_PREPARE,
  NET_SERVER_QM_QUERY_EXECUTE,
  NET_SERVER_QM_QUERY_PREPARE_AND_EXECUTE,
  NET_SERVER_QM_QUERY_END,
  NET_SERVER_QM_QUERY_DROP_PLAN,
  NET_SERVER_QM_QUERY_DROP_ALL_PLANS,
  NET_SERVER_QM_QUERY_SYNC,
  NET_SERVER_QM_GET_QUERY_INFO,
  NET_SERVER_QM_QUERY_EXECUTE_ASYNC,
  NET_SERVER_QM_QUERY_PREPARE_AND_EXECUTE_ASYNC,
  NET_SERVER_QM_QUERY_DUMP_PLANS,
  NET_SERVER_QM_QUERY_DUMP_CACHE,

  NET_SERVER_LS_GET_LIST_FILE_PAGE,

  NET_SERVER_MNT_SERVER_START_STATS,
  NET_SERVER_MNT_SERVER_STOP_STATS,
  NET_SERVER_MNT_SERVER_RESET_STATS,
  NET_SERVER_MNT_SERVER_COPY_STATS,

  NET_SERVER_CT_CAN_ACCEPT_NEW_REPR,

  NET_SERVER_CSS_KILL_TRANSACTION,

  NET_SERVER_QPROC_GET_SYS_TIMESTAMP,
  NET_SERVER_QPROC_GET_CURRENT_VALUE,
  NET_SERVER_QPROC_GET_NEXT_VALUE,
  NET_SERVER_QPROC_GET_SERVER_INFO,

  NET_SERVER_PRM_SET_PARAMETERS,
  NET_SERVER_PRM_GET_PARAMETERS,
  NET_SERVER_PRM_DUMP_PARAMETERS,

  NET_SERVER_JSP_GET_SERVER_PORT,

  NET_SERVER_REPL_INFO,
  NET_SERVER_REPL_LOG_GET_APPEND_LSA,

  NET_SERVER_LOGWR_GET_LOG_PAGES,

  NET_SERVER_TEST_PERFORMANCE,

  NET_SERVER_SHUTDOWN,

  NET_SERVER_REPL_BTREE_FIND_UNIQUE,

  NET_SERVER_MNT_SERVER_COPY_GLOBAL_STATS,
  NET_SERVER_MNT_SERVER_RESET_GLOBAL_STATS,

  NET_SERVER_CSS_DUMP_CS_STAT,

  /*
   * This is the last entry. It is also used for the end of an
   * array of statistics information on client/server communication.
   */
  NET_SERVER_REQUEST_END,
  /*
   * This request number must be preserved.
   */
  NET_SERVER_PING_WITH_HANDSHAKE = 999,
};

/* Server/client capabilities */
#define NET_CAP_BACKWARD_COMPATIBLE     0x80000000
#define NET_CAP_FORWARD_COMPATIBLE      0x40000000
#define NET_CAP_INTERRUPT_ENABLED       0x00800000
#define NET_CAP_UPDATE_DISABLED         0x00008000
#define NET_CAP_REMOTE_DISABLED         0x00000080

extern char *net_pack_stats (char *buf, MNT_SERVER_EXEC_STATS * stats);
extern char *net_unpack_stats (char *buf, MNT_SERVER_EXEC_STATS * stats);

extern char *net_pack_global_stats (char *buf,
				    MNT_SERVER_EXEC_GLOBAL_STATS * stats);
extern char *net_unpack_global_stats (char *buf,
				      MNT_SERVER_EXEC_GLOBAL_STATS * stats);

/* Server startup */
extern int net_server_start (const char *name);

#endif /* _NETWORK_H_ */
