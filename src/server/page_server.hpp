/*
 * Copyright 2008 Search Solution Corporation
 * Copyright 2016 CUBRID Corporation
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 */

#ifndef _PAGE_SERVER_HPP_
#define _PAGE_SERVER_HPP_

#include "log_replication.hpp"
#include "log_storage.hpp"
#include "request_sync_client_server.hpp"
#include "server_request_responder.hpp"
#include "server_type_enum.hpp"
#include "tran_page_requests.hpp"
#include "async_disconnect_handler.hpp"

#include <future>
#include <memory>

/* Sequence diagram of server-server communication:
 *
 *                                                 #
 *                                     TRAN SERVER # PAGE SERVER
 *                                                 #
 *                 ┌───────────────────────────┐   #   ┌────────────────────────────────────────────┐
 *                 │request_sync_client_server │   #   │connection_handler                          │
 *    ┌────┐  (1)  │                           │   #   │                                            │
 * ┌──► Ti ├───────┼─────┐                     │   #   │ ┌───────────────────────────┐              │
 * │  └────┘       │     │                     │   #   │ │request_sync_client_server │              │
 * │               │     │   ┌────────────┐    │   #   │ │(per tran server)          │  ┌─────────┐ │
 * │               │     ├───►  send      │    │(2)#   │ │   ┌─────────────┐  (3)    │  │server   │ │
 * │  ┌────┐ (1)   │     │   │  thread    ├────┼───────┼─┼───►  receive    ├─────────┼──►request  │ │
 * ├──► Tx ├───────┼─────┤   └────────────┘    │   #   │ │   │   thread    │         │  │responder│ │
 * │  └────┘       │     │                     │   #   │ │   └─────────────┘         │  │thread   │ │
 * │               │     │                     │   #   │ │                       (4) │  │pool     │ │
 * │               │     │     ┌──────────┐    │   #   │ │                      ┌────┼──┤         │ │
 * │  ┌────┐  (1)  │     │     │ receive  │    │   #(5)│ │   ┌─────────────┐    │    │  └─────────┘ │
 * ├──► Td ├───────┼─────┘     │  thread  ◄────┼───────┼─┼───┤   send      │    │    │              │
 * │  └────┘       │           └─┬────────┘    │   #   │ │   │   thread    ◄────┘    │              │
 * │               │             │             │   #   │ │   └─────────────┘         │              │
 * │               └─────────────┼─────────────┘   #   │ │                           │              │
 * │       (6)                   │                 #   │ └───────────────────────────┘              │
 * └─────────────────────────────┘                 #   │                                            │
 *                                                 #   └────────────────────────────────────────────┘
 *                                                 #
 *
 * (1)  transactions or system threads in a transaction server produce requests which require resources from
 *      page server (eg: heap pages, log pages)
 * (2)  these requests are serialized into requests for a send thread (request_sync_client_server,
 *      request_sync_send_queue, request_queue_autosend) - the actual send thread is instantiated in
 *      request_queue_autosend - which then sends requests over the network;
 *      there are two two types of requests
 *      - that wait for a response from the other side
 *      - fire&forget messages (eg: log prior messages being sent from active transaction server to page server)
 * (3)  on the page server messages are processed by a receive thread (request_sync_client_server -
 *      request_client_server) with the receive thread actually being instantiated in request_client_server;
 *      the receiving thread has a handler map which, together with the message's request id results in a
 *      certain handler function to be called
 *      received messages are being processed in two modes:
 *      - synchronously: some messages make no sense to be processed asynchronously so they are processed
 *        directly within the connection handler instance
 *      - asynchronously: connection handler dispatches the message for processing and response retrieval
 *        to a thread pool via task (server_request_responder)
 * (4)  the async server request responder then redirects the response to the sending thread of page server
 * (5)  which sends it over the network to the transaction server side
 * (6)  the receive thread on the transaction server side use also the message's request id and a handler map
 *      to know which waiting thread to actually wake to consume the response
 */

class page_server
{
  public:
    page_server (const char *db_name);
    page_server (const page_server &) = delete;
    page_server (page_server &&) = delete;

    ~page_server ();

    page_server &operator = (const page_server &) = delete;
    page_server &operator = (page_server &&) = delete;

    void set_active_tran_server_connection (cubcomm::channel &&chn);
    void set_passive_tran_server_connection (cubcomm::channel &&chn);
    void set_follower_page_server_connection (cubcomm::channel &&chn);
    void disconnect_all_tran_servers ();
    void disconnect_followee_page_server (bool with_disc_msg);
    void disconnect_all_follower_page_servers ();

    int connect_to_followee_page_server (std::string &&hostname, int32_t port);

    void push_request_to_active_tran_server (page_to_tran_request reqid, std::string &&payload);
    cublog::replicator &get_replicator ();
    void start_log_replicator (const log_lsa &start_lsa);
    void finish_replication_during_shutdown (cubthread::entry &thread_entry);

    void init_request_responder ();
    void finalize_request_responder ();

  private: // types
    class tran_server_connection_handler
    {
      public:
	using tran_server_conn_t =
		cubcomm::request_sync_client_server<page_to_tran_request, tran_to_page_request, std::string>;

	tran_server_connection_handler () = delete;
	tran_server_connection_handler (cubcomm::channel &&chn, transaction_server_type server_type, page_server &ps);

	tran_server_connection_handler (const tran_server_connection_handler &) = delete;
	tran_server_connection_handler (tran_server_connection_handler &&) = delete;

	~tran_server_connection_handler ();

	tran_server_connection_handler &operator= (const tran_server_connection_handler &) = delete;
	tran_server_connection_handler &operator= (tran_server_connection_handler &&) = delete;

	void push_request (page_to_tran_request id, std::string &&msg);
	const std::string &get_connection_id () const;

	void remove_prior_sender_sink ();

	// request disconnection of this connection (TS)
	void push_disconnection_request ();

      private:
	// Request handlers for the request server:
	void receive_boot_info_request (tran_server_conn_t::sequenced_payload &&a_sp);
	void receive_log_page_fetch (tran_server_conn_t::sequenced_payload &&a_sp);
	void receive_data_page_fetch (tran_server_conn_t::sequenced_payload &&a_sp);
	void receive_disconnect_request (tran_server_conn_t::sequenced_payload &&a_sp);
	void receive_log_prior_list (tran_server_conn_t::sequenced_payload &&a_sp);
	void handle_oldest_active_mvccid_request (tran_server_conn_t::sequenced_payload &&a_sp);
	void receive_log_boot_info_fetch (tran_server_conn_t::sequenced_payload &&a_sp);
	void receive_stop_log_prior_dispatch (tran_server_conn_t::sequenced_payload &&a_sp);
	void receive_oldest_active_mvccid (tran_server_conn_t::sequenced_payload &&a_sp);
	void receive_start_catch_up (tran_server_conn_t::sequenced_payload &&a_sp);

	void abnormal_tran_server_disconnect (css_error_code error_code, bool &abort_further_processing);

	// Helper function to convert above functions into responder specific tasks.
	template<class F, class ... Args>
	void push_async_response (F &&, tran_server_conn_t::sequenced_payload &&a_sp, Args &&...args);

	// Function used as sink for log transfer
	void prior_sender_sink_hook (std::string &&message) const;

      private:
	/* there is another mode in which the connection handler for active transaction server
	 * can be differentiated from the connection handler for passive transaction server: the
	 * presence of prior sender sink hook function pointer below;
	 * however, at some point, the hook function will be removed - following a request from
	 * the peer transaction server and the check will no longer be valid
	 */
	const transaction_server_type m_server_type;
	const std::string m_connection_id;

	std::unique_ptr<tran_server_conn_t> m_conn;
	page_server &m_ps;

	// only passive transaction servers receive log in the form of prior list;
	cublog::prior_sender::sink_hook_t m_prior_sender_sink_hook_func;

	// exclusive lock between the hook function that executes the dispatch and the
	// function that will, at some moment, remove that hook
	mutable std::mutex m_prior_sender_sink_removal_mtx;

	std::mutex m_abnormal_tran_server_disconnect_mtx;
	bool m_abnormal_tran_server_disconnect;
    };

    /*
     *  When a disconnected page server re-connects to a cluster, the cluster, possibly,
     *  has gone further than the page server. It means the connecting PS has log records only up to LSA(N),
     *  but the ATS and other PSes in the cluster have log records up to a LSN larger than LSN(N) like LSN(N+1000).
     *  Because ATS sends log records and just discards them, the connecting PS must get the pages from another PS
     *  to fill the hole.
     *
     *  Terms:
     *  - catchup_lsa: the log lsa where new log records from ATS start to come in. A PS must have log records
     *    until this point. If it doesn’t have, it must fetch log pages until that point via the catch-up mechanism.
     *    Note that the catchup_lsa is different depending on when a PS connects to an ATS.
     *  - Follower: a page server who is reconnecting to a cluster with log records in the past. It must fill the log
     *    records hole from where the end of log records it has, to catchup_lsa. It fetches log pages from a followee.
     *  - Followee: a page server who serves missing log pages to a follower. It’s selected by ATS. A Followee can have
     *    multiple followers at a time. And there can be multiple followees at a time as well.

     *  When a page server connects to an ATS in a cluster:
     *
     *         ┌─────┐   ┌──────────┐       ┌──────────┐
     *         │ ats │-->│ follower │ <---> │ followee │
     *         └─────┘   └──────────┘       └──────────┘
     *
     *  1. A page server boots up and becomes a follower.
     *  2. ATS selects a followee page server and sends the node information and the catchup_lsa to the follower.
     *  3. The follower makes a temporary connection with the followee.
     *  4. The follower requests log pages to fill the log hole ranging [start_lsa, catchup_lsa)
     *     - start_lsa: the append_lsa of the follower, where it expects a new log record will be appended.
     *     - catchup_lsa: the lsa of the first log record that the ATS will send.
     *     -> The follower fills the log hole [start_lsa, catchup_lsa) to accept new log records from its ATS.
     *  5. After the catch-up is completed, the follower notifies to its ATS.
     *  6. The follower destroys the temporary connection.
     *  7. The follower PS continues by processing log records received from ATS in the interval [catchup_lsa, ....);
     *
     *  Note that
     *  - A followee can have multiple followers at a time.
     *  - There can be multiple followees in a cluster at a time.
     *  - Followee is a kind of server, which just serves what Follower wants.
     *  - If a follower stops or is killed during the catch-up, it can restart the catch-up from where it stops
     *    if the fetched log pages have been flushed on disk.
     *  - Even while a follower is catching up with a followee, it can also receive new log records from ATS.
     *    They are kept and going to be appended after the catch-up is completed.
     *
     *  TODO
     *  - It could be that log volumes in a followee that contain requested log pages have been removed when requested.
     *    In this case, the follower who requested should be re-initiate the catch-up with another PS.
     *  - It could be that log volumes in a followee that contain requested log pages can be removed while catching up.
     *    In this case, the follower who requested should be re-initiate the catch-up with another PS.
     *  - With other communication errors by its followee, a follower should re-initiate the catch-up with another PS.
     *  - If the amount of log pages to catch up with is too much, it would be better
     *    to copy data volumes and log volumes directly without the catch-up and replicating.
     *  - The log records from ATS while the catch-up are kept on memory in transient. It can consume all memory
     *    and overflow. This should be addressed. I think we could expand the catch-up range and discard some kept
     *    log records with a threshold.
     *  - We can make the replicator and the catch-up process in parallel.
     */
    class follower_connection_handler
    {
      public:
	using follower_server_conn_t =
		cubcomm::request_sync_client_server<followee_to_follower_request, follower_to_followee_request, std::string>;

	follower_connection_handler () = delete;
	follower_connection_handler (cubcomm::channel &&chn, page_server &ps);

	follower_connection_handler (const follower_connection_handler &) = delete;
	follower_connection_handler (follower_connection_handler &&) = delete;

	follower_connection_handler &operator= (const follower_connection_handler &) = delete;
	follower_connection_handler &operator= (follower_connection_handler &&) = delete;

	const std::string get_channel_id () const;

	~follower_connection_handler ();

      private:
	void receive_log_pages_fetch (follower_server_conn_t::sequenced_payload &&a_sp);
	void receive_disconnect_request (follower_server_conn_t::sequenced_payload &&a_sp);

	void serve_log_pages (THREAD_ENTRY &, std::string &payload_in_out);

	void send_error_handler (css_error_code error_code, bool &abort_further_processing);
	void recv_error_handler (css_error_code error_code);

	page_server &m_ps;
	std::unique_ptr<follower_server_conn_t> m_conn;
    };

    class followee_connection_handler
    {
      public:
	followee_connection_handler () = delete;
	followee_connection_handler (cubcomm::channel &&chn, page_server &ps);

	followee_connection_handler (const followee_connection_handler &) = delete;
	followee_connection_handler (followee_connection_handler &&) = delete;

	followee_connection_handler &operator= (const followee_connection_handler &) = delete;
	followee_connection_handler &operator= (followee_connection_handler &&) = delete;

	const std::string get_channel_id () const;

	int request_log_pages (LOG_PAGEID start_pageid, int count, const std::vector<LOG_PAGE *> &log_pages_out);

	void push_request (follower_to_followee_request reqid, std::string &&msg);

      private:
	using followee_server_conn_t =
		cubcomm::request_sync_client_server<follower_to_followee_request, followee_to_follower_request, std::string>;

	int send_receive (follower_to_followee_request reqid, std::string &&payload_in, std::string &payload_out);

	void send_error_handler (css_error_code error_code, bool &abort_further_processing);
	void recv_error_handler (css_error_code error_code);

      private:
	page_server &m_ps;
	std::unique_ptr<followee_server_conn_t> m_conn;
    };

    /*
     * helper class to track the active oldest mvccids of each Page Transaction Server.
     * This provides the globally oldest active mvcc id to the vacuum on ATS.
     * The vacuum has to take mvcc status of all PTSes into considerations,
     * or it would clean up some data seen by a active snapshot on a PTS.
     */
    class pts_mvcc_tracker
    {
      public:
	pts_mvcc_tracker () = default;

	pts_mvcc_tracker (const pts_mvcc_tracker &) = delete;
	pts_mvcc_tracker (pts_mvcc_tracker &&) = delete;

	pts_mvcc_tracker &operator = (const pts_mvcc_tracker &) = delete;
	pts_mvcc_tracker &operator = (pts_mvcc_tracker &&) = delete;

	void init_oldest_active_mvccid (const std::string &pts_channel_id);
	void update_oldest_active_mvccid (const std::string &pts_channel_id, const MVCCID mvccid);
	void delete_oldest_active_mvccid (const std::string &pts_channel_id);

	MVCCID get_global_oldest_active_mvccid ();

      private:
	/* <channel_id -> the oldest active mvccid of the PTS>. used by the vacuum on the ATS */
	std::unordered_map<std::string, MVCCID> m_pts_oldest_active_mvccids;
	std::mutex m_pts_oldest_active_mvccids_mtx;
    };

    using tran_server_connection_handler_uptr_t = std::unique_ptr<tran_server_connection_handler>;
    using follower_connection_handler_uptr_t = std::unique_ptr<follower_connection_handler>;
    using followee_connection_handler_uptr_t = std::unique_ptr<followee_connection_handler>;

    using tran_server_responder_t = server_request_responder<tran_server_connection_handler::tran_server_conn_t>;
    using follower_responder_t = server_request_responder<follower_connection_handler::follower_server_conn_t>;

  private: // functions that depend on private types
    void disconnect_active_tran_server ();
    void disconnect_tran_server_async (const tran_server_connection_handler *conn);
    void disconnect_follower_page_server_async (const follower_connection_handler *conn);

    bool is_active_tran_server_connected () const;

    void start_catchup (const LOG_LSA catchup_lsa);
    void execute_catchup (cubthread::entry &entry, const LOG_LSA catchup_lsa);
    void complete_catchup ();

    tran_server_responder_t &get_tran_server_responder ();
    follower_responder_t &get_follower_responder ();

  private: // members
    const std::string m_server_name;

    tran_server_connection_handler_uptr_t m_active_tran_server_conn;
    std::vector<tran_server_connection_handler_uptr_t> m_passive_tran_server_conn;
    std::mutex m_conn_mutex; // for the thread-safe connection and disconnection
    std::condition_variable m_conn_cv;

    std::unique_ptr<cublog::replicator> m_replicator;

    std::unique_ptr<tran_server_responder_t> m_tran_server_responder;
    std::unique_ptr<follower_responder_t> m_follower_responder;

    async_disconnect_handler<tran_server_connection_handler> m_async_disconnect_handler;
    pts_mvcc_tracker m_pts_mvcc_tracker;

    followee_connection_handler_uptr_t m_followee_conn;
    std::vector<follower_connection_handler_uptr_t> m_follower_conn_vec;

    std::mutex m_followee_conn_mutex;
    std::mutex m_follower_conn_vec_mutex;
    std::condition_variable m_follower_conn_vec_cv;
    std::future<void> m_follower_disc_future;
    std::mutex m_follower_disc_mutex;

    /*
     * A worker_pool to take some asynchronous jobs that need a thread entry.
     * It's used by two server_request_responders for tran servers and follower servers,
     * and some extra jobs like the catch-up.
     *
     * Note that both tran_server_responder and follower_responder hold pointers to the worker pool.
     * Thus, they must be destroyed prior to the worker pool - to avoid dangling pointers.
     * The responders ensure that all jobs are waited for completion in their dtors.
     */
    cubthread::system_worker_entry_manager m_worker_context_manager;
    cubthread::entry_workpool *m_worker_pool;
};

#endif // !_PAGE_SERVER_HPP_
