/*
 * Copyright (c) 2004, 2005 Voltaire, Inc. All rights reserved.
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
 * 	Declaration of osm_vlarb_rec_rcv_t.
 *	This object represents the VLArbitration Record Receiver object.
 *	attribute from a node.
 *	This object is part of the OpenSM family of objects.
 *
 * Environment:
 * 	Linux User Mode
 *
 * $Revision: 1.3 $
 */

#ifndef _OSM_VLARB_REC_RCV_H_
#define _OSM_VLARB_REC_RCV_H_

#include <complib/cl_passivelock.h>
#include <complib/cl_qlist.h>
#include <opensm/osm_base.h>
#include <opensm/osm_madw.h>
#include <opensm/osm_sa_response.h>
#include <opensm/osm_subnet.h>
#include <opensm/osm_port.h>
#include <opensm/osm_log.h>

#ifdef __cplusplus
#  define BEGIN_C_DECLS extern "C" {
#  define END_C_DECLS   }
#else				/* !__cplusplus */
#  define BEGIN_C_DECLS
#  define END_C_DECLS
#endif				/* __cplusplus */

BEGIN_C_DECLS
/****h* OpenSM/VLArbitration Record Receiver
* NAME
*	VLArbitration Record Receiver
*
* DESCRIPTION
*	The VLArbitration Record Receiver object encapsulates the information
*	needed to handle VL Arbitration Record query from a SA.
*
*	The VLArbitration Record Receiver object is thread safe.
*
*	This object should be treated as opaque and should be
*	manipulated only through the provided functions.
*
* AUTHOR
*	Eitan Zahavi, Mellanox
*
*********/
/****s* OpenSM: VLArbitration Record Receiver/osm_vlarb_rec_rcv_t
* NAME
*	osm_vlarb_rec_rcv_t
*
* DESCRIPTION
*	VLArbitration Record Receiver structure.
*
*	This object should be treated as opaque and should
*	be manipulated only through the provided functions.
*
* SYNOPSIS
*/
typedef struct _osm_vlarb_rec_rcv {
	osm_subn_t *p_subn;
	osm_sa_resp_t *p_resp;
	osm_mad_pool_t *p_mad_pool;
	osm_log_t *p_log;
	cl_plock_t *p_lock;
	cl_qlock_pool_t pool;
} osm_vlarb_rec_rcv_t;
/*
* FIELDS
*	p_subn
*		Pointer to the Subnet object for this subnet.
*
*	p_resp
*		Pointer to the SA responder.
*
*	p_mad_pool
*		Pointer to the mad pool.
*
*	p_log
*		Pointer to the log object.
*
*	p_lock
*		Pointer to the serializing lock.
*
*	pool
*		Pool of linkable VLArbitration Record objects used to generate
*		the query response.
*
* SEE ALSO
*
*********/

/****f* OpenSM: VLArbitration Record Receiver/osm_vlarb_rec_rcv_construct
* NAME
*	osm_vlarb_rec_rcv_construct
*
* DESCRIPTION
*	This function constructs a VLArbitration Record Receiver object.
*
* SYNOPSIS
*/
void osm_vlarb_rec_rcv_construct(IN osm_vlarb_rec_rcv_t * const p_rcv);
/*
* PARAMETERS
*	p_rcv
*		[in] Pointer to a VLArbitration Record Receiver object to construct.
*
* RETURN VALUE
*	This function does not return a value.
*
* NOTES
*	Allows calling osm_vlarb_rec_rcv_init, osm_vlarb_rec_rcv_destroy
*
*	Calling osm_vlarb_rec_rcv_construct is a prerequisite to calling any other
*	method except osm_vlarb_rec_rcv_init.
*
* SEE ALSO
*	VLArbitration Record Receiver object, osm_vlarb_rec_rcv_init,
*	osm_vlarb_rec_rcv_destroy
*********/

/****f* OpenSM: VLArbitration Record Receiver/osm_vlarb_rec_rcv_destroy
* NAME
*	osm_vlarb_rec_rcv_destroy
*
* DESCRIPTION
*	The osm_vlarb_rec_rcv_destroy function destroys the object, releasing
*	all resources.
*
* SYNOPSIS
*/
void osm_vlarb_rec_rcv_destroy(IN osm_vlarb_rec_rcv_t * const p_rcv);
/*
* PARAMETERS
*	p_rcv
*		[in] Pointer to the object to destroy.
*
* RETURN VALUE
*	This function does not return a value.
*
* NOTES
*	Performs any necessary cleanup of the specified
*	VLArbitration Record Receiver object.
*	Further operations should not be attempted on the destroyed object.
*	This function should only be called after a call to
*	osm_vlarb_rec_rcv_construct or osm_vlarb_rec_rcv_init.
*
* SEE ALSO
*	VLArbitration Record Receiver object, osm_vlarb_rec_rcv_construct,
*	osm_vlarb_rec_rcv_init
*********/

/****f* OpenSM: VLArbitration Record Receiver/osm_vlarb_rec_rcv_init
* NAME
*	osm_vlarb_rec_rcv_init
*
* DESCRIPTION
*	The osm_vlarb_rec_rcv_init function initializes a
*	VLArbitration Record Receiver object for use.
*
* SYNOPSIS
*/
ib_api_status_t
osm_vlarb_rec_rcv_init(IN osm_vlarb_rec_rcv_t * const p_rcv,
		       IN osm_sa_resp_t * const p_resp,
		       IN osm_mad_pool_t * const p_mad_pool,
		       IN osm_subn_t * const p_subn,
		       IN osm_log_t * const p_log,
		       IN cl_plock_t * const p_lock);
/*
* PARAMETERS
*	p_rcv
*		[in] Pointer to an osm_vlarb_rec_rcv_t object to initialize.
*
*	p_req
*		[in] Pointer to an osm_req_t object.
*
*	p_subn
*		[in] Pointer to the Subnet object for this subnet.
*
*	p_log
*		[in] Pointer to the log object.
*
*	p_lock
*		[in] Pointer to the OpenSM serializing lock.
*
* RETURN VALUES
*	CL_SUCCESS if the VLArbitration Record Receiver object was initialized
*	successfully.
*
* NOTES
*	Allows calling other VLArbitration Record Receiver methods.
*
* SEE ALSO
*	VLArbitration Record Receiver object, osm_vlarb_rec_rcv_construct,
*	osm_vlarb_rec_rcv_destroy
*********/

/****f* OpenSM: VLArbitration Record Receiver/osm_vlarb_rec_rcv_process
* NAME
*	osm_vlarb_rec_rcv_process
*
* DESCRIPTION
*	Process the VL Arbitration Table Query .
*
* SYNOPSIS
*/
void osm_vlarb_rec_rcv_process(IN void *context, IN void *data);
/*
* PARAMETERS
*	context
*		[in] Pointer to an osm_vlarb_rec_rcv_t object.
*
*	data
*		[in] Pointer to the MAD Wrapper containing the MAD
*		that contains the VL Arbitration Record Query attribute.
*
* RETURN VALUES
*	CL_SUCCESS if the Query processing was successful.
*
* NOTES
*	This function processes a SA VL Arbitration Record attribute.
*
* SEE ALSO
*	VLArbitration Record Receiver, VLArbitration Record Response Controller
*********/

END_C_DECLS
#endif				/* _OSM_VLARB_REC_RCV_H_ */
