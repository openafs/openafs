=head1 Copyright

 # Copyright 2006, Sine Nomine Associates and others.
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

#include <osi/osi_impl.h>
#include <osi/osi_trace.h>
#include <trace/rpc/agent_api.h>
#include <trace/console/probe_client.h>
#include <trace/console/init.h>
#include <trace/console/trap_queue.h>


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

	if (SvROK(addr_ref) &&
	    (SvTYPE(SvRV(addr_ref)) == SVt_PVHV) &&
	    (sv_isobject(addr_ref)) &&
	    (sv_derived_from(addr_ref, "osi::trace::console::proc"))) {
		addr = (HV *) SvRV(addr_ref);
		val = hv_fetch(addr, "__gen_id", 8, 0);
		if (val == NULL) {
			goto failure;
		}
		if (!SvIOK(*val)) {
			goto failure;
		}
		*addr_out = SvUV(*val);
		RETVAL = OSI_OK;
	}

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

	if (SvROK(addr_ref) &&
	    (SvTYPE(SvRV(addr_ref)) == SVt_PVHV) &&
	    (sv_isobject(addr_ref)) &&
	    (sv_derived_from(addr_ref, "osi::trace::console::proc"))) {
		addr = (HV *) SvRV(addr_ref);
		hv_store(addr, "__gen_id", 8, newSVuv(*addr_in), 0);
		RETVAL = OSI_OK;
	}

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

	internal function to convert an osi_trace_console_trap_t into a
	trap perl object

=cut

osi_result
osi__trace__console__trap___c2p(osi_trace_console_trap_t * trap_in, 
				SV * trap_ref)
{
	osi_result RETVAL = OSI_FAIL;
	HV * trap;
	SV * val;

	if (SvROK(trap_ref) &&
	    (SvTYPE(SvRV(trap_ref)) == SVt_PVHV) &&
	    (sv_isobject(trap_ref)) &&
	    (sv_derived_from(trap_ref, "osi::trace::console::trap"))) {
		trap = (HV *) SvRV(trap_ref);
		hv_store(trap, "__encoding", 10, newSVuv(trap_in->trap_encoding), 0);
		hv_store(trap, "__version", 9, newSVuv(trap_in->trap_version), 0);
		hv_store(trap, "__payload", 9, newSVpv(trap_in->trap_payload.osi_Trace_rpc_trap_data_val,
						       trap_in->trap_payload.osi_Trace_rpc_trap_data_len), 0);
		RETVAL = OSI_OK;
	}

	return RETVAL;
}


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
    const char * filter_ref
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
		HV * trap;
		osi_trace_console_trap_t * trap_in;
	CODE:
		RETVAL = osi_trace_console_trap_queue_get(&trap_in);
		if (OSI_RESULT_OK_LIKELY(RETVAL)) {
			RETVAL = osi__trace__console__trap___c2p(trap_in, trap_ref);
			(void)osi_trace_console_trap_queue_put(trap_in);
		}


MODULE = osi::trace::console	PACKAGE = osi::trace::console::agent	PREFIX=osi__trace__console__agent__

BOOT:
	SV * val;
	val = get_sv("osi::trace::console::agent::TRANSPORT_RX_UDP", TRUE);
	sv_setuv(val, OSI_TRACE_RPC_CONSOLE_TRANSPORT_RX_UDP);
	val = get_sv("osi::trace::console::agent::TRANSPORT_RX_TCP", TRUE);
	sv_setuv(val, OSI_TRACE_RPC_CONSOLE_TRANSPORT_RX_TCP);
	val = get_sv("osi::trace::console::agent::TRANSPORT_SNMP_V1", TRUE);
	sv_setuv(val, OSI_TRACE_RPC_CONSOLE_TRANSPORT_SNMP_V1);
