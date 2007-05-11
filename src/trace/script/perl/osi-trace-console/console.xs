=head1 Copyright

 # Copyright 2006-2007, Sine Nomine Associates and others.
 # All Rights Reserved.
 #
 # This software has been released under the terms of the IBM Public
 # License.  For details, see the LICENSE file in the top-level source
 # directory or online at http://www.openafs.org/dl/license10.html

=cut


#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "ppport.h"

#include <trace/common/trace_impl.h>
#include <trace/rpc/agent_api.h>
#include <trace/console/probe_client.h>
#include <trace/console/init.h>
#include <trace/console/trap_queue.h>
#include <trace/console/proc_client.h>
#include <trace/encoding/handle.h>

=pod

	the following are static routines to do conversion between
	the various perl objects we support, and the C data types
	which are expected by the various libosi_trace_* libraries

=cut

=pod

	internal function to convert an agent handle perl object
	into an osi_Trace_rpc_addr4 structure

=cut

osi_result
osi__trace__console__agent___p2c(SV * addr_ref, osi_Trace_rpc_addr4 * addr_out)
{
	osi_result RETVAL = OSI_FAIL;
	HV * addr;
	SV ** val;

	if (SvROK(addr_ref) && 
	    (SvTYPE(SvRV(addr_ref)) == SVt_PVHV) &&
	    (sv_isobject(addr_ref)) &&
	    (sv_derived_from(addr_ref, "osi::trace::console::agent"))) {
		addr = (HV *) SvRV(addr_ref);
		val = hv_fetch(addr, "__addr", 6, 0);
		if (val == NULL) {
			goto failure;
		}
		if (!SvIOK(*val)) {
			goto failure;
		}
		addr_out->addr = SvUV(*val);
		val = hv_fetch(addr, "__port", 6, 0);
		if (val == NULL) {
			goto failure;
		}
		if (!SvIOK(*val)) {
			goto failure;
		}
		addr_out->port = SvUV(*val);
		val = hv_fetch(addr, "__transport", 11, 0);
		if (val == NULL) {
			goto failure;
		}
		if (!SvIOK(*val)) {
			goto failure;
		}
		addr_out->transport = SvUV(*val);
		RETVAL = OSI_OK;
	}

 failure:
	return RETVAL;
}

=pod

	internal function to convert an osi_Trace_rpc_addr4 structure
	into an agent handle perl object

=cut

osi_result
osi__trace__console__agent___c2p(osi_Trace_rpc_addr4 * addr_in, SV * addr_ref)
{
	osi_result RETVAL = OSI_FAIL;
	HV * addr;

	if (SvROK(addr_ref) && 
	    (SvTYPE(SvRV(addr_ref)) == SVt_PVHV) &&
	    (sv_isobject(addr_ref)) &&
	    (sv_derived_from(addr_ref, "osi::trace::console::agent"))) {
		addr = (HV *) SvRV(addr_ref);
		hv_store(addr, "__addr", 6, newSVuv(addr_in->addr), 0);
		hv_store(addr, "__port", 6, newSVuv(addr_in->port), 0);
		hv_store(addr, "__transport", 11, newSVuv(addr_in->transport), 0);
		RETVAL = OSI_OK;
	}

	return RETVAL;
}

=pod

	internal function to deref a perl ref to a proc object into
	an HV pointer

=cut

osi_static osi_inline osi_result
osi__trace__console__proc___r2h(SV * proc_ref,
				HV ** proc)
{
	osi_result res = OSI_FAIL;

	if (SvROK(proc_ref) &&
	    (SvTYPE(SvRV(proc_ref)) == SVt_PVHV) &&
	    (sv_isobject(proc_ref)) &&
	    (sv_derived_from(proc_ref, "osi::trace::console::proc"))) {
		*proc = (HV *) SvRV(proc_ref);
		res = OSI_OK;
	}

	return res;
}

=pod

	internal function to convert a process handle perl object into
	an osi_Trace_rpc_proc_handle C type

=cut

osi_result
osi__trace__console__proc___p2c(SV * addr_ref, 
				osi_Trace_rpc_proc_handle * addr_out)
{
	osi_result RETVAL = OSI_FAIL;
	HV * addr;
	SV ** val;

	RETVAL = osi__trace__console__proc___r2h(addr_ref, &addr);
	if (OSI_RESULT_FAIL_UNLIKELY(RETVAL)) {
		goto failure;
	}

	val = hv_fetch(addr, "__gen_id", 8, 0);
	if (val == NULL) {
		goto failure;
	}
	if (!SvIOK(*val)) {
		goto failure;
	}
	*addr_out = SvUV(*val);
	RETVAL = OSI_OK;

 failure:
	return RETVAL;
}

=pod

	internal function to convert an osi_Trace_rpc_proc_handle C type
	into a process handle perl object 

=cut

osi_result
osi__trace__console__proc___c2p(osi_Trace_rpc_proc_handle * addr_in, 
				SV * addr_ref)
{
	osi_result RETVAL = OSI_FAIL;
	HV * addr;

	RETVAL = osi__trace__console__proc___r2h(addr_ref, &addr);
	if (OSI_RESULT_FAIL_UNLIKELY(RETVAL)) {
		goto failure;
	}

	hv_store(addr, "__gen_id", 8, newSVuv(*addr_in), 0);
	RETVAL = OSI_OK;

 failure:
	return RETVAL;
}

=pod

	internal function to convert an osi_Trace_rpc_proc_info C type
	into a process handle perl object

=cut

osi_result
osi__trace__console__proc__info___c2p(osi_Trace_rpc_proc_info * info,
				      SV * proc_ref)
{
	osi_result RETVAL = OSI_FAIL;
	HV * proc;
	AV * arr;
	SV * val;
	unsigned int i;

	RETVAL = osi__trace__console__proc___r2h(proc_ref, &proc);
	if (OSI_RESULT_FAIL_UNLIKELY(RETVAL)) {
		goto failure;
	}

	hv_store(proc, "__gen_id", 8, newSVuv(info->handle), 0);
	hv_store(proc, "__i_pid", 7, newSVuv(info->handle), 0);
	hv_store(proc, "__i_ptype", 9, newSVuv(info->handle), 0);
	hv_store(proc, "__i_version", 11, newSVuv(info->handle), 0);

	arr = newAV();
	if (arr == NULL) {
		goto failure;
	}
	av_unshift(arr, 8);

	for (i = 0; i < 8; i++) {
		av_store(arr, i, newSVuv(info->capabilities[i]));
	}

	val = newRV_noinc((SV *)arr);
	hv_store(proc, "__i_caps", 8, val, 0);
	RETVAL = OSI_OK;

 failure:
	return RETVAL;
}

=pod

	internal function to convert a filter perl object into
	a const char *

=cut

osi_result
osi__trace__console__filter___p2c(SV * filter_ref, char ** filter_out)
{
	osi_result RETVAL = OSI_FAIL;
	HV * filter;
	SV ** val;

	if (SvROK(filter_ref) &&
	    (SvTYPE(SvRV(filter_ref)) == SVt_PVHV) &&
	    (sv_isobject(filter_ref)) &&
	    (sv_derived_from(filter_ref, "osi::trace::console::filter"))) {
		filter = (HV *) SvRV(filter_ref);
		val = hv_fetch(filter, "__filter", 8, 0);
		if (val == NULL) {
			goto failure;
		}
		if (!SvPOK(*val)) {
			goto failure;
		}
		*filter_out = SvPV_nolen(*val);
		RETVAL = OSI_OK;
	}

 failure:
	return RETVAL;
}

=pod

	internal function to deref a perl ref to a trap object into
	an HV pointer

=cut

osi_static osi_inline osi_result
osi__trace__console__trap___r2h(SV * trap_ref,
				HV ** trap)
{
	osi_result res = OSI_FAIL;

	if (SvROK(trap_ref) &&
	    (SvTYPE(SvRV(trap_ref)) == SVt_PVHV) &&
	    (sv_isobject(trap_ref)) &&
	    (sv_derived_from(trap_ref, "osi::trace::console::trap"))) {
		*trap = (HV *) SvRV(trap_ref);
		res = OSI_OK;
	}

	return res;
}

=pod

	accessors for the encoding, version, and payload fields

=cut

osi_result
osi__trace__console__trap__get_encoding(HV * trap,
					osi_trace_trap_encoding_t * encoding)
{
	osi_result res = OSI_FAIL;
	SV ** val;

	val = hv_fetch(trap, "__encoding", 10, 0);
	if (val == NULL) {
		goto failure;
	}
	if (!SvIOK(*val)) {
		goto failure;
	}
	*encoding = (osi_trace_trap_encoding_t) SvUV(*val);
	res = OSI_OK;

  failure:
	return res;
}

osi_result
osi__trace__console__trap__get_version(HV * trap,
				       osi_uint32 * version)
{
	osi_result res = OSI_FAIL;
	SV ** val;

	val = hv_fetch(trap, "__version", 9, 0);
	if (val == NULL) {
		goto failure;
	}
	if (!SvIOK(*val)) {
		goto failure;
	}
	*version = (osi_uint32) SvUV(*val);
	res = OSI_OK;

  failure:
	return res;
}

osi_result
osi__trace__console__trap__get_payload(HV * trap,
				       void ** payload,
				       osi_size_t * payload_len)
{
	osi_result res = OSI_FAIL;
	SV ** val;
	osi_size_t len;

	val = hv_fetch(trap, "__payload", 9, 0);
	if (val == NULL) {
		goto failure;
	}
	if (!SvIOK(*val)) {
		goto failure;
	}
	*payload = SvPV(*val, len);
	*payload_len = len;
	res = OSI_OK;

  failure:
	return res;
}

=pod

	internal function to convert an osi_trace_console_trap_t into a
	trap perl object

=cut

osi_result
osi__trace__console__trap___c2p(osi_trace_console_trap_t * trap_in, 
				SV * trap_ref)
{
	osi_result res = OSI_FAIL;
	HV * trap;

	res = osi__trace__console__trap___r2h(trap_ref, &trap);
	if (OSI_RESULT_FAIL_UNLIKELY(res)) {
		goto error;
	}
	
	hv_store(trap, "__encoding", 10, newSVuv(trap_in->trap_encoding), 0);
	hv_store(trap, "__version", 9, newSVuv(trap_in->trap_version), 0);
	hv_store(trap, "__payload", 9, newSVpv(trap_in->trap_payload.osi_Trace_rpc_trap_data_val,
					       trap_in->trap_payload.osi_Trace_rpc_trap_data_len), 0);

  error:
	return res;
}

=pod

	internal function to convert an osi_TracePoint_record into the
	the payload part of the trap perl object

=cut

osi_result
osi__trace__console__trap__payload___c2p(HV * trap,
					 osi_TracePoint_record_ptr_t record_ptr)
{
	osi_result res = OSI_FAIL;
	osi_TracePoint_record_v3 * record;
	AV * arr;
	SV * val;

	if (record_ptr.version != OSI_TRACEPOINT_RECORD_VERSION_V3) {
		goto error;
	}
	record = record_ptr.ptr.v3;

	arr = newAV();
	if (arr == NULL) {
		goto error;
	}
	av_unshift(arr, 2);

	av_store(arr, 0, newSVuv(record->timestamp.tv_sec));
	av_store(arr, 1, newSVuv(record->timestamp.tv_nsec));

	val = newRV_noinc((SV *)arr);
	hv_store(trap, "__p_timestamp", 13, val, 0);

	hv_store(trap, "__p_pid", 7, newSVuv(record->pid), 0);
	hv_store(trap, "__p_tid", 7, newSVuv(record->tid), 0);
	hv_store(trap, "__p_version", 11, newSVuv(record->version), 0);
	hv_store(trap, "__p_nargs", 9, newSVuv(record->nargs), 0);

	arr = newAV();
	if (arr == NULL) {
		goto error;
	}
	av_unshift(arr, 2);

	av_store(arr, 0, newSVuv(record->tags[0]));
	av_store(arr, 1, newSVuv(record->tags[1]));

	val = newRV_noinc((SV *)arr);
	hv_store(trap, "__p_tags", 8, val, 0);

	hv_store(trap, "__p_probe_id", 12, newSVuv(record->probe), 0);
	hv_store(trap, "__p_gen_id", 10, newSVuv(record->gen_id), 0);
	hv_store(trap, "__p_cpu_id", 10, newSVuv(record->cpu_id), 0);

	arr = newAV();
	if (arr == NULL) {
		goto error;
	}
	if (record->nargs) {
		unsigned int i;

		av_unshift(arr, OSI_TRACEPOINT_PAYLOAD_VEC_LEN);

		for (i = 0; i < OSI_TRACEPOINT_PAYLOAD_VEC_LEN; i++) {
			av_store(arr, i, newSVuv(record->payload[i]));
		}
	}
	val = newRV_noinc((SV *)arr);
	hv_store(trap, "__p_payload", 11, val, 0);

  error:
	return res;
}

=pod

	internal function to call into the trap encoding library to decode
	an incoming trap

=cut

osi_result
osi__trace__console__trap__payload___decode(SV * trap_ref)
{
	osi_result res;
	osi_trace_trap_enc_handle_t * handle = osi_NULL;
	void * payload;
	osi_size_t payload_len;
	osi_trace_trap_encoding_t encoding;
	osi_uint32 version;
	HV * trap;
	osi_TracePoint_record_ptr_t record;

	res = osi__trace__console__trap___r2h(trap_ref, &trap);
	if (OSI_RESULT_FAIL_UNLIKELY(res)) {
		goto error;
	}

	res = osi__trace__console__trap___get_encoding(trap, &encoding);
	if (OSI_RESULT_FAIL_UNLIKELY(res)) {
		goto error;
	}

	res = osi__trace__console__trap___get_version(trap, &version);
	if (OSI_RESULT_FAIL_UNLIKELY(res)) {
		goto error;
	}

	res = osi__trace__console__trap___get_payload(trap, &payload, &payload_len);
	if (OSI_RESULT_FAIL_UNLIKELY(res)) {
		goto error;
	}

	res = osi_trace_trap_enc_handle_alloc(OSI_TRACE_TRAP_ENC_DIRECTION_DECODE,
					      encoding,
					      &handle);
	if (OSI_RESULT_FAIL_UNLIKELY(res)) {
		goto error;
	}

	res = osi_trace_trap_enc_handle_setup_decode(handle,
						     payload,
						     payload_len,
						     version,
						     OSI_TRACEPOINT_RECORD_VERSION_V3);
	if (OSI_RESULT_FAIL_UNLIKELY(res)) {
		goto error;
	}

	res = osi_trace_trap_enc_handle_decode(handle);
	if (OSI_RESULT_FAIL_UNLIKELY(res)) {
		goto error;
	}

	res = osi_trace_trap_enc_handle_get_decoding(handle,
						     &record);
	if (OSI_RESULT_FAIL_UNLIKELY(res)) {
		goto error;
	}

	res = osi__trace__console__trap__payload___c2p(trap,
						       record);

  error:
	if (handle != osi_NULL) {
		osi_trace_trap_enc_handle_put(handle);
	}

	return res;					      	
}



=pod

	from here on down are the XS interfaces

=cut


MODULE = osi::trace::console		PACKAGE = osi::trace::console		PREFIX=osi__trace__console__

BOOT:
# bootstrap the console library
	osi_Assert(OSI_RESULT_OK(osi_PkgInit(osi_ProgramType_Utility, osi_NULL)));
	osi_Assert(OSI_RESULT_OK(osi_trace_console_PkgInit()));
	osi_Assert(OSI_RESULT_OK(osi_trace_console_trap_queue_enable()));

# provide for explicit library initialization and finalization,
# just in case someone wants to do something weird
osi_result
osi__trace__console___init(void)
	CODE:
		RETVAL = osi_PkgInit(osi_ProgramType_EphemeralUtility, osi_NULL);
		if (OSI_RESULT_OK(RETVAL)) {
			RETVAL = osi_trace_console_PkgInit();
			if (OSI_RESULT_OK(RETVAL)) {
				RETVAL = osi_trace_console_trap_queue_enable();
			}
		}

osi_result
osi__trace__console___fini(void)
	CODE:
		RETVAL = osi_trace_console_trap_queue_disable();
		if (OSI_RESULT_OK(RETVAL)) {
			RETVAL = osi_trace_console_PkgShutdown();
			if (OSI_RESULT_OK(RETVAL)) {
				RETVAL = osi_PkgShutdown();
			}
		}


void
osi__trace__console__probe_enable(addr_ref, proc_handle_ref, filter_ref)
    SV * addr_ref
    SV * proc_handle_ref
    SV * filter_ref
	INIT:
		osi_Trace_rpc_addr4 addr;
		osi_Trace_rpc_proc_handle proc_handle;
		char * filter;
		osi_result res;
		osi_uint32 nhits;
	PPCODE:
		res = osi__trace__console__agent___p2c(addr_ref, &addr);
		if (OSI_RESULT_FAIL(res)) {
			croak("osi::trace::console::probe_disable argument 1 not a valid osi::trace::console::agent");
		}
		res = osi__trace__console__proc___p2c(proc_handle_ref, &proc_handle);
		if (OSI_RESULT_FAIL(res)) {
			croak("osi::trace::console::probe_disable argument 2 not a valid osi::trace::console::proc");
		}
		res = osi__trace__console__filter___p2c(filter_ref, &filter);
		if (OSI_RESULT_FAIL(res)) {
			croak("osi::trace::console::probe_disable argument 3 not a valid osi::trace::console::filter");
		}
		res = osi_trace_console_probe_enable(&addr, proc_handle, filter, &nhits);
		ST(0) = newSViv(res);
		sv_2mortal(ST(0));
		ST(1) = newSVuv(nhits);
		sv_2mortal(ST(1));
		XSRETURN(2);


void
osi__trace__console__probe_disable(addr_ref, proc_handle_ref, filter_ref)
    SV * addr_ref
    SV * proc_handle_ref
    SV * filter_ref
	INIT:
		osi_Trace_rpc_addr4 addr;
		osi_Trace_rpc_proc_handle proc_handle;
		char * filter;
		osi_result res;
		osi_uint32 nhits;
	PPCODE:
		res = osi__trace__console__agent___p2c(addr_ref, &addr);
		if (OSI_RESULT_FAIL(res)) {
			croak("osi::trace::console::probe_disable argument 1 not a valid osi::trace::console::agent");
		}
		res = osi__trace__console__proc___p2c(proc_handle_ref, &proc_handle);
		if (OSI_RESULT_FAIL(res)) {
			croak("osi::trace::console::probe_disable argument 2 not a valid osi::trace::console::proc");
		}
		res = osi__trace__console__filter___p2c(filter_ref, &filter);
		if (OSI_RESULT_FAIL(res)) {
			croak("osi::trace::console::probe_disable argument 3 not a valid osi::trace::console::filter");
		}
		res = osi_trace_console_probe_disable(&addr, proc_handle, filter, &nhits);
		ST(0) = newSViv(res);
		sv_2mortal(ST(0));
		ST(1) = newSVuv(nhits);
		sv_2mortal(ST(1));
		XSRETURN(2);

osi_result
osi__trace__console__trap_get(trap_ref)
    SV * trap_ref
	INIT:
		osi_trace_console_trap_t * trap_in;
	CODE:
		RETVAL = osi_trace_console_trap_queue_get(&trap_in);
		if (OSI_RESULT_OK_LIKELY(RETVAL)) {
			RETVAL = osi__trace__console__trap___c2p(trap_in, trap_ref);
			(void)osi_trace_console_trap_queue_put(trap_in);
		}


MODULE = osi::trace::console	PACKAGE = osi::trace::console::agent	PREFIX=osi__trace__console__agent__

BOOT:
	SV * osi__trace__console__agent__val;
	osi__trace__console__agent__val = get_sv("osi::trace::console::agent::TRANSPORT_RX_UDP", TRUE);
	sv_setuv(osi__trace__console__agent__val, OSI_TRACE_RPC_CONSOLE_TRANSPORT_RX_UDP);
	osi__trace__console__agent__val = get_sv("osi::trace::console::agent::TRANSPORT_RX_TCP", TRUE);
	sv_setuv(osi__trace__console__agent__val, OSI_TRACE_RPC_CONSOLE_TRANSPORT_RX_TCP);
	osi__trace__console__agent__val = get_sv("osi::trace::console::agent::TRANSPORT_SNMP_V1", TRUE);
	sv_setuv(osi__trace__console__agent__val, OSI_TRACE_RPC_CONSOLE_TRANSPORT_SNMP_V1);


MODULE = osi::trace::console	PACKAGE = osi::trace::console::proc	PREFIX=osi__trace__console__proc__

BOOT:
	SV * osi__trace__console__proc__val;
	osi__trace__console__proc__val = get_sv("osi::trace::console::proc::ProgramType_Library", TRUE);
	sv_setuv(osi__trace__console__proc__val, osi_ProgramType_Library);
	osi__trace__console__proc__val = get_sv("osi::trace::console::proc::ProgramType_BosServer", TRUE);
	sv_setuv(osi__trace__console__proc__val, osi_ProgramType_Bosserver);
	osi__trace__console__proc__val = get_sv("osi::trace::console::proc::ProgramType_CacheManager", TRUE);
	sv_setuv(osi__trace__console__proc__val, osi_ProgramType_CacheManager);
	osi__trace__console__proc__val = get_sv("osi::trace::console::proc::ProgramType_FileServer", TRUE);
	sv_setuv(osi__trace__console__proc__val, osi_ProgramType_Fileserver);
	osi__trace__console__proc__val = get_sv("osi::trace::console::proc::ProgramType_VolServer", TRUE);
	sv_setuv(osi__trace__console__proc__val, osi_ProgramType_Volserver);
	osi__trace__console__proc__val = get_sv("osi::trace::console::proc::ProgramType_Salvager", TRUE);
	sv_setuv(osi__trace__console__proc__val, osi_ProgramType_Salvager);
	osi__trace__console__proc__val = get_sv("osi::trace::console::proc::ProgramType_SalvageServer", TRUE);
	sv_setuv(osi__trace__console__proc__val, osi_ProgramType_Salvageserver);
	osi__trace__console__proc__val = get_sv("osi::trace::console::proc::ProgramType_SalvageServerWorker", TRUE);
	sv_setuv(osi__trace__console__proc__val, osi_ProgramType_SalvageserverWorker);
	osi__trace__console__proc__val = get_sv("osi::trace::console::proc::ProgramType_PtServer", TRUE);
	sv_setuv(osi__trace__console__proc__val, osi_ProgramType_Ptserver);
	osi__trace__console__proc__val = get_sv("osi::trace::console::proc::ProgramType_VLServer", TRUE);
	sv_setuv(osi__trace__console__proc__val, osi_ProgramType_Vlserver);
	osi__trace__console__proc__val = get_sv("osi::trace::console::proc::ProgramType_KAServer", TRUE);
	sv_setuv(osi__trace__console__proc__val, osi_ProgramType_Kaserver);
	osi__trace__console__proc__val = get_sv("osi::trace::console::proc::ProgramType_BuServer", TRUE);
	sv_setuv(osi__trace__console__proc__val, osi_ProgramType_Buserver);
	osi__trace__console__proc__val = get_sv("osi::trace::console::proc::ProgramType_Utility", TRUE);
	sv_setuv(osi__trace__console__proc__val, osi_ProgramType_Utility);
	osi__trace__console__proc__val = get_sv("osi::trace::console::proc::ProgramType_EphemeralUtility", TRUE);
	sv_setuv(osi__trace__console__proc__val, osi_ProgramType_EphemeralUtility);
	osi__trace__console__proc__val = get_sv("osi::trace::console::proc::ProgramType_TraceCollector", TRUE);
	sv_setuv(osi__trace__console__proc__val, osi_ProgramType_TraceCollector);
	osi__trace__console__proc__val = get_sv("osi::trace::console::proc::ProgramType_TestSuite", TRUE);
	sv_setuv(osi__trace__console__proc__val, osi_ProgramType_TestSuite);
	osi__trace__console__proc__val = get_sv("osi::trace::console::proc::ProgramType_TraceKernel", TRUE);
	sv_setuv(osi__trace__console__proc__val, osi_ProgramType_TraceKernel);
	osi__trace__console__proc__val = get_sv("osi::trace::console::proc::ProgramType_Backup", TRUE);
	sv_setuv(osi__trace__console__proc__val, osi_ProgramType_Backup);
	osi__trace__console__proc__val = get_sv("osi::trace::console::proc::ProgramType_BuTC", TRUE);
	sv_setuv(osi__trace__console__proc__val, osi_ProgramType_BuTC);
	osi__trace__console__proc__val = get_sv("osi::trace::console::proc::ProgramType_UpServer", TRUE);
	sv_setuv(osi__trace__console__proc__val, osi_ProgramType_UpServer);
	osi__trace__console__proc__val = get_sv("osi::trace::console::proc::ProgramType_UpClient", TRUE);
	sv_setuv(osi__trace__console__proc__val, osi_ProgramType_UpClient);
	osi__trace__console__proc__val = get_sv("osi::trace::console::proc::ProgramType_Bos", TRUE);
	sv_setuv(osi__trace__console__proc__val, osi_ProgramType_Bos);
	osi__trace__console__proc__val = get_sv("osi::trace::console::proc::ProgramType_Vos", TRUE);
	sv_setuv(osi__trace__console__proc__val, osi_ProgramType_Vos);
	osi__trace__console__proc__val = get_sv("osi::trace::console::proc::ProgramType_AFSD", TRUE);
	sv_setuv(osi__trace__console__proc__val, osi_ProgramType_AFSD);
	osi__trace__console__proc__val = get_sv("osi::trace::console::proc::ProgramType_RMTSYSD", TRUE);
	sv_setuv(osi__trace__console__proc__val, osi_ProgramType_RMTSYSD);

osi_result
osi__trace__console__proc__agent(addr_ref, proc_ref)
    SV * addr_ref
    SV * proc_ref
	INIT:
		osi_Trace_rpc_addr4 addr;
		osi_Trace_rpc_proc_handle proc;
	PPCODE:
		RETVAL = osi__trace__console__agent___p2c(addr_ref, &addr);
		if (OSI_RESULT_FAIL(RETVAL)) {
			croak("osi::trace::console::probe_disable argument 1 not a valid osi::trace::console::agent");
		}
		RETVAL = osi_trace_console_proc_agent(&addr, &proc);
		if (OSI_RESULT_OK(RETVAL)) {
			RETVAL = osi__trace__console__proc___c2p(&proc, proc_ref);
		}

osi_result
osi__trace__console__proc__info(addr_ref, proc_ref)
    SV * addr_ref
    SV * proc_ref
	INIT:
		osi_Trace_rpc_addr4 addr;
		osi_Trace_rpc_proc_handle proc;
		osi_Trace_rpc_proc_info info;
	PPCODE:
		RETVAL = osi__trace__console__agent___p2c(addr_ref, &addr);
		if (OSI_RESULT_FAIL(RETVAL)) {
			croak("osi::trace::console::probe_disable argument 1 not a valid osi::trace::console::agent");
		}
		RETVAL = osi__trace__console__proc___p2c(proc_ref, &proc);
		if (OSI_RESULT_FAIL(RETVAL)) {
			croak("osi::trace::console::probe_disable argument 2 not a valid osi::trace::console::proc");
		}
		RETVAL = osi_trace_console_proc_info(&addr, proc, &info);
		if (OSI_RESULT_OK(RETVAL)) {
			RETVAL = osi__trace__console__proc__info___c2p(&info, proc_ref);
		}

osi_result
osi__trace__console__proc__list(addr_ref, proc_ref)
    SV * addr_ref
    SV * proc_ref
	INIT:
		osi_Trace_rpc_addr4 addr;
		osi_Trace_rpc_proc_handle proc;
		osi_Trace_rpc_proc_info info;
	PPCODE:
		RETVAL = osi__trace__console__agent___p2c(addr_ref, &addr);
		if (OSI_RESULT_FAIL(RETVAL)) {
			croak("osi::trace::console::probe_disable argument 1 not a valid osi::trace::console::agent");
		}
		RETVAL = osi__trace__console__proc___p2c(proc_ref, &proc);
		if (OSI_RESULT_FAIL(RETVAL)) {
			croak("osi::trace::console::probe_disable argument 2 not a valid osi::trace::console::proc");
		}
		RETVAL = osi_trace_console_proc_info(&addr, proc, &info);
		if (OSI_RESULT_OK(RETVAL)) {
			RETVAL = osi__trace__console__proc__info___c2p(&info, proc_ref);
		}


MODULE = osi::trace::console	PACKAGE = osi::trace::console::trap	PREFIX=osi__trace__console__trap__

osi_result
osi__trace__console__trap__decode(trap_ref)
    SV * trap_ref
	INIT:
		osi_trace_console_trap_t * trap_in;
	CODE:
		RETVAL = osi_trace_console_trap_queue_get(&trap_in);
		if (OSI_RESULT_OK_LIKELY(RETVAL)) {
			RETVAL = osi__trace__console__trap___c2p(trap_in, trap_ref);
			(void)osi_trace_console_trap_queue_put(trap_in);
		}


