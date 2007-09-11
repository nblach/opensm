/*
 * Copyright (c) 2004-2007 Voltaire, Inc. All rights reserved.
 * Copyright (c) 2002-2005 Mellanox Technologies LTD. All rights reserved.
 * Copyright (c) 1996-2003 Intel Corporation. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

/*
 * Abstract:
 *    Implementation of osm_lftr_rcv_t.
 *   This object represents the LinearForwardingTable Receiver object.
 *   This object is part of the opensm family of objects.
 *
 * Environment:
 *    Linux User Mode
 *
 * $Revision: 1.5 $
 */

#if HAVE_CONFIG_H
#  include <config.h>
#endif				/* HAVE_CONFIG_H */

#include <string.h>
#include <iba/ib_types.h>
#include <complib/cl_debug.h>
#include <complib/cl_qlist.h>
#include <opensm/osm_sa_lft_record.h>
#include <opensm/osm_switch.h>
#include <vendor/osm_vendor_api.h>
#include <opensm/osm_helper.h>
#include <opensm/osm_pkey.h>
#include <opensm/osm_sa.h>

#define OSM_LFTR_RCV_POOL_MIN_SIZE      32
#define OSM_LFTR_RCV_POOL_GROW_SIZE     32

typedef struct _osm_lftr_item {
	cl_pool_item_t pool_item;
	ib_lft_record_t rec;
} osm_lftr_item_t;

typedef struct _osm_lftr_search_ctxt {
	const ib_lft_record_t *p_rcvd_rec;
	ib_net64_t comp_mask;
	cl_qlist_t *p_list;
	osm_lftr_rcv_t *p_rcv;
	const osm_physp_t *p_req_physp;
} osm_lftr_search_ctxt_t;

/**********************************************************************
 **********************************************************************/
void osm_lftr_rcv_construct(IN osm_lftr_rcv_t * const p_rcv)
{
	memset(p_rcv, 0, sizeof(*p_rcv));
	cl_qlock_pool_construct(&p_rcv->pool);
}

/**********************************************************************
 **********************************************************************/
void osm_lftr_rcv_destroy(IN osm_lftr_rcv_t * const p_rcv)
{
	OSM_LOG_ENTER(p_rcv->p_log, osm_lftr_rcv_destroy);
	cl_qlock_pool_destroy(&p_rcv->pool);
	OSM_LOG_EXIT(p_rcv->p_log);
}

/**********************************************************************
 **********************************************************************/
ib_api_status_t
osm_lftr_rcv_init(IN osm_lftr_rcv_t * const p_rcv,
		  IN osm_sa_resp_t * const p_resp,
		  IN osm_mad_pool_t * const p_mad_pool,
		  IN osm_subn_t * const p_subn,
		  IN osm_log_t * const p_log, IN cl_plock_t * const p_lock)
{
	ib_api_status_t status;

	OSM_LOG_ENTER(p_log, osm_lftr_rcv_init);

	osm_lftr_rcv_construct(p_rcv);

	p_rcv->p_log = p_log;
	p_rcv->p_subn = p_subn;
	p_rcv->p_lock = p_lock;
	p_rcv->p_resp = p_resp;
	p_rcv->p_mad_pool = p_mad_pool;

	status = cl_qlock_pool_init(&p_rcv->pool,
				    OSM_LFTR_RCV_POOL_MIN_SIZE,
				    0,
				    OSM_LFTR_RCV_POOL_GROW_SIZE,
				    sizeof(osm_lftr_item_t), NULL, NULL, NULL);

	OSM_LOG_EXIT(p_log);
	return (status);
}

/**********************************************************************
 **********************************************************************/
static ib_api_status_t
__osm_lftr_rcv_new_lftr(IN osm_lftr_rcv_t * const p_rcv,
			IN const osm_switch_t * const p_sw,
			IN cl_qlist_t * const p_list,
			IN ib_net16_t const lid, IN ib_net16_t const block)
{
	osm_lftr_item_t *p_rec_item;
	ib_api_status_t status = IB_SUCCESS;

	OSM_LOG_ENTER(p_rcv->p_log, __osm_lftr_rcv_new_lftr);

	p_rec_item = (osm_lftr_item_t *) cl_qlock_pool_get(&p_rcv->pool);
	if (p_rec_item == NULL) {
		osm_log(p_rcv->p_log, OSM_LOG_ERROR,
			"__osm_lftr_rcv_new_lftr: ERR 4402: "
			"cl_qlock_pool_get failed\n");
		status = IB_INSUFFICIENT_RESOURCES;
		goto Exit;
	}

	if (osm_log_is_active(p_rcv->p_log, OSM_LOG_DEBUG)) {
		osm_log(p_rcv->p_log, OSM_LOG_DEBUG,
			"__osm_lftr_rcv_new_lftr: "
			"New LinearForwardingTable: sw 0x%016" PRIx64
			"\n\t\t\t\tblock 0x%02X lid 0x%02X\n",
			cl_ntoh64(osm_node_get_node_guid(p_sw->p_node)),
			cl_ntoh16(block), cl_ntoh16(lid)
		    );
	}

	memset(&p_rec_item->rec, 0, sizeof(ib_lft_record_t));

	p_rec_item->rec.lid = lid;
	p_rec_item->rec.block_num = block;

	/* copy the lft block */
	osm_switch_get_fwd_tbl_block(p_sw, cl_ntoh16(block),
				     p_rec_item->rec.lft);

	cl_qlist_insert_tail(p_list,
			     (cl_list_item_t *) & p_rec_item->pool_item);

      Exit:
	OSM_LOG_EXIT(p_rcv->p_log);
	return (status);
}

/**********************************************************************
 **********************************************************************/
static osm_port_t *__osm_lftr_get_port_by_guid(IN osm_lftr_rcv_t * const p_rcv,
					       IN uint64_t port_guid)
{
	osm_port_t *p_port;

	CL_PLOCK_ACQUIRE(p_rcv->p_lock);

	p_port = osm_get_port_by_guid(p_rcv->p_subn, port_guid);
	if (!p_port) {
		osm_log(p_rcv->p_log, OSM_LOG_DEBUG,
			"__osm_lftr_get_port_by_guid ERR 4404: "
			"Invalid port GUID 0x%016" PRIx64 "\n", port_guid);
		p_port = NULL;
	}

	CL_PLOCK_RELEASE(p_rcv->p_lock);
	return p_port;
}

/**********************************************************************
 **********************************************************************/
static void
__osm_lftr_rcv_by_comp_mask(IN cl_map_item_t * const p_map_item,
			    IN void *context)
{
	const osm_lftr_search_ctxt_t *const p_ctxt =
	    (osm_lftr_search_ctxt_t *) context;
	const osm_switch_t *const p_sw = (osm_switch_t *) p_map_item;
	const ib_lft_record_t *const p_rcvd_rec = p_ctxt->p_rcvd_rec;
	osm_lftr_rcv_t *const p_rcv = p_ctxt->p_rcv;
	ib_net64_t const comp_mask = p_ctxt->comp_mask;
	const osm_physp_t *const p_req_physp = p_ctxt->p_req_physp;
	osm_port_t *p_port;
	uint16_t min_lid_ho, max_lid_ho;
	uint16_t min_block, max_block, block;
	const osm_physp_t *p_physp;

	/* In switches, the port guid is the node guid. */
	p_port =
	    __osm_lftr_get_port_by_guid(p_rcv,
					p_sw->p_node->node_info.port_guid);
	if (!p_port) {
		osm_log(p_rcv->p_log, OSM_LOG_ERROR,
			"__osm_lftr_rcv_by_comp_mask: ERR 4405: "
			"Failed to find Port by Node Guid:0x%016" PRIx64
			"\n", cl_ntoh64(p_sw->p_node->node_info.node_guid)
		    );
		return;
	}

	/* check that the requester physp and the current physp are under
	   the same partition. */
	p_physp = p_port->p_physp;
	if (!p_physp) {
		osm_log(p_rcv->p_log, OSM_LOG_ERROR,
			"__osm_lftr_rcv_by_comp_mask: ERR 4406: "
			"Failed to find default physical Port by Node Guid:0x%016"
			PRIx64 "\n",
			cl_ntoh64(p_sw->p_node->node_info.node_guid)
		    );
		return;
	}
	if (!osm_physp_share_pkey(p_rcv->p_log, p_req_physp, p_physp))
		return;

	/* get the port 0 of the switch */
	osm_port_get_lid_range_ho(p_port, &min_lid_ho, &max_lid_ho);

	/* compare the lids - if required */
	if (comp_mask & IB_LFTR_COMPMASK_LID) {
		osm_log(p_rcv->p_log, OSM_LOG_DEBUG,
			"__osm_lftr_rcv_by_comp_mask: "
			"Comparing lid:0x%02X to port lid range: 0x%02X .. 0x%02X\n",
			cl_ntoh16(p_rcvd_rec->lid), min_lid_ho, max_lid_ho);
		/* ok we are ready for range check */
		if (min_lid_ho > cl_ntoh16(p_rcvd_rec->lid) ||
		    max_lid_ho < cl_ntoh16(p_rcvd_rec->lid))
			return;
	}

	/* now we need to decide which blocks to output */
	if (comp_mask & IB_LFTR_COMPMASK_BLOCK) {
		max_block = min_block = cl_ntoh16(p_rcvd_rec->block_num);
	} else {
		/* use as many blocks as "in use" */
		min_block = 0;
		max_block = osm_switch_get_max_block_id_in_use(p_sw);
	}

	/* so we can add these blocks one by one ... */
	for (block = min_block; block <= max_block; block++)
		__osm_lftr_rcv_new_lftr(p_rcv, p_sw, p_ctxt->p_list,
					osm_port_get_base_lid(p_port),
					cl_hton16(block));
}

/**********************************************************************
 **********************************************************************/
void osm_lftr_rcv_process(IN void *ctx, IN void *data)
{
	osm_lftr_rcv_t *p_rcv = ctx;
	osm_madw_t *p_madw = data;
	const ib_sa_mad_t *p_rcvd_mad;
	const ib_lft_record_t *p_rcvd_rec;
	ib_lft_record_t *p_resp_rec;
	cl_qlist_t rec_list;
	osm_madw_t *p_resp_madw;
	ib_sa_mad_t *p_resp_sa_mad;
	uint32_t num_rec, pre_trim_num_rec;
#ifndef VENDOR_RMPP_SUPPORT
	uint32_t trim_num_rec;
#endif
	uint32_t i;
	osm_lftr_search_ctxt_t context;
	osm_lftr_item_t *p_rec_item;
	ib_api_status_t status = IB_SUCCESS;
	osm_physp_t *p_req_physp;

	CL_ASSERT(p_rcv);

	OSM_LOG_ENTER(p_rcv->p_log, osm_lftr_rcv_process);

	CL_ASSERT(p_madw);

	p_rcvd_mad = osm_madw_get_sa_mad_ptr(p_madw);
	p_rcvd_rec = (ib_lft_record_t *) ib_sa_mad_get_payload_ptr(p_rcvd_mad);

	CL_ASSERT(p_rcvd_mad->attr_id == IB_MAD_ATTR_LFT_RECORD);

	/* we only support SubnAdmGet and SubnAdmGetTable methods */
	if ((p_rcvd_mad->method != IB_MAD_METHOD_GET) &&
	    (p_rcvd_mad->method != IB_MAD_METHOD_GETTABLE)) {
		osm_log(p_rcv->p_log, OSM_LOG_ERROR,
			"osm_lftr_rcv_process: ERR 4408: "
			"Unsupported Method (%s)\n",
			ib_get_sa_method_str(p_rcvd_mad->method));
		osm_sa_send_error(p_rcv->p_resp, p_madw,
				  IB_MAD_STATUS_UNSUP_METHOD_ATTR);
		goto Exit;
	}

	/* update the requester physical port. */
	p_req_physp = osm_get_physp_by_mad_addr(p_rcv->p_log,
						p_rcv->p_subn,
						osm_madw_get_mad_addr_ptr
						(p_madw));
	if (p_req_physp == NULL) {
		osm_log(p_rcv->p_log, OSM_LOG_ERROR,
			"osm_lftr_rcv_process: ERR 4407: "
			"Cannot find requester physical port\n");
		goto Exit;
	}

	cl_qlist_init(&rec_list);

	context.p_rcvd_rec = p_rcvd_rec;
	context.p_list = &rec_list;
	context.comp_mask = p_rcvd_mad->comp_mask;
	context.p_rcv = p_rcv;
	context.p_req_physp = p_req_physp;

	cl_plock_acquire(p_rcv->p_lock);

	/* Go over all switches */
	cl_qmap_apply_func(&p_rcv->p_subn->sw_guid_tbl,
			   __osm_lftr_rcv_by_comp_mask, &context);

	cl_plock_release(p_rcv->p_lock);

	num_rec = cl_qlist_count(&rec_list);

	/*
	 * C15-0.1.30:
	 * If we do a SubnAdmGet and got more than one record it is an error !
	 */
	if (p_rcvd_mad->method == IB_MAD_METHOD_GET) {
		if (num_rec == 0) {
			osm_sa_send_error(p_rcv->p_resp, p_madw,
					  IB_SA_MAD_STATUS_NO_RECORDS);
			goto Exit;
		}
		if (num_rec > 1) {
			osm_log(p_rcv->p_log, OSM_LOG_ERROR,
				"osm_lftr_rcv_process: ERR 4409: "
				"Got more than one record for SubnAdmGet (%u)\n",
				num_rec);
			osm_sa_send_error(p_rcv->p_resp, p_madw,
					  IB_SA_MAD_STATUS_TOO_MANY_RECORDS);

			/* need to set the mem free ... */
			p_rec_item =
			    (osm_lftr_item_t *) cl_qlist_remove_head(&rec_list);
			while (p_rec_item !=
			       (osm_lftr_item_t *) cl_qlist_end(&rec_list)) {
				cl_qlock_pool_put(&p_rcv->pool,
						  &p_rec_item->pool_item);
				p_rec_item =
				    (osm_lftr_item_t *)
				    cl_qlist_remove_head(&rec_list);
			}

			goto Exit;
		}
	}

	pre_trim_num_rec = num_rec;
#ifndef VENDOR_RMPP_SUPPORT
	/* we limit the number of records to a single packet */
	trim_num_rec =
	    (MAD_BLOCK_SIZE - IB_SA_MAD_HDR_SIZE) / sizeof(ib_lft_record_t);
	if (trim_num_rec < num_rec) {
		osm_log(p_rcv->p_log, OSM_LOG_VERBOSE,
			"osm_lftr_rcv_process: "
			"Number of records:%u trimmed to:%u to fit in one MAD\n",
			num_rec, trim_num_rec);
		num_rec = trim_num_rec;
	}
#endif

	osm_log(p_rcv->p_log, OSM_LOG_DEBUG,
		"osm_lftr_rcv_process: " "Returning %u records\n", num_rec);

	if ((p_rcvd_mad->method != IB_MAD_METHOD_GETTABLE) && (num_rec == 0)) {
		osm_sa_send_error(p_rcv->p_resp, p_madw,
				  IB_SA_MAD_STATUS_NO_RECORDS);
		goto Exit;
	}

	/*
	 * Get a MAD to reply. Address of Mad is in the received mad_wrapper
	 */
	p_resp_madw = osm_mad_pool_get(p_rcv->p_mad_pool,
				       p_madw->h_bind,
				       num_rec * sizeof(ib_lft_record_t) +
				       IB_SA_MAD_HDR_SIZE, &p_madw->mad_addr);

	if (!p_resp_madw) {
		osm_log(p_rcv->p_log, OSM_LOG_ERROR,
			"osm_lftr_rcv_process: ERR 4410: "
			"osm_mad_pool_get failed\n");

		for (i = 0; i < num_rec; i++) {
			p_rec_item =
			    (osm_lftr_item_t *) cl_qlist_remove_head(&rec_list);
			cl_qlock_pool_put(&p_rcv->pool, &p_rec_item->pool_item);
		}

		osm_sa_send_error(p_rcv->p_resp, p_madw,
				  IB_SA_MAD_STATUS_NO_RESOURCES);

		goto Exit;
	}

	p_resp_sa_mad = osm_madw_get_sa_mad_ptr(p_resp_madw);

	/*
	   Copy the MAD header back into the response mad.
	   Set the 'R' bit and the payload length,
	   Then copy all records from the list into the response payload.
	 */

	memcpy(p_resp_sa_mad, p_rcvd_mad, IB_SA_MAD_HDR_SIZE);
	p_resp_sa_mad->method |= IB_MAD_METHOD_RESP_MASK;
	/* C15-0.1.5 - always return SM_Key = 0 (table 185 p 884) */
	p_resp_sa_mad->sm_key = 0;
	/* Fill in the offset (paylen will be done by the rmpp SAR) */
	p_resp_sa_mad->attr_offset =
	    ib_get_attr_offset(sizeof(ib_lft_record_t));

	p_resp_rec =
	    (ib_lft_record_t *) ib_sa_mad_get_payload_ptr(p_resp_sa_mad);

#ifndef VENDOR_RMPP_SUPPORT
	/* we support only one packet RMPP - so we will set the first and
	   last flags for gettable */
	if (p_resp_sa_mad->method == IB_MAD_METHOD_GETTABLE_RESP) {
		p_resp_sa_mad->rmpp_type = IB_RMPP_TYPE_DATA;
		p_resp_sa_mad->rmpp_flags =
		    IB_RMPP_FLAG_FIRST | IB_RMPP_FLAG_LAST |
		    IB_RMPP_FLAG_ACTIVE;
	}
#else
	/* forcefully define the packet as RMPP one */
	if (p_resp_sa_mad->method == IB_MAD_METHOD_GETTABLE_RESP)
		p_resp_sa_mad->rmpp_flags = IB_RMPP_FLAG_ACTIVE;
#endif

	for (i = 0; i < pre_trim_num_rec; i++) {
		p_rec_item =
		    (osm_lftr_item_t *) cl_qlist_remove_head(&rec_list);
		/* copy only if not trimmed */
		if (i < num_rec) {
			*p_resp_rec = p_rec_item->rec;
		}
		cl_qlock_pool_put(&p_rcv->pool, &p_rec_item->pool_item);
		p_resp_rec++;
	}

	CL_ASSERT(cl_is_qlist_empty(&rec_list));

	status = osm_sa_vendor_send(p_resp_madw->h_bind, p_resp_madw, FALSE,
				    p_rcv->p_subn);
	if (status != IB_SUCCESS) {
		osm_log(p_rcv->p_log, OSM_LOG_ERROR,
			"osm_lftr_rcv_process: ERR 4411: "
			"osm_sa_vendor_send status = %s\n",
			ib_get_err_str(status));
		goto Exit;
	}

      Exit:
	OSM_LOG_EXIT(p_rcv->p_log);
}
