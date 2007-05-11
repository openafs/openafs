#
# Copyright 2006-2007, Sine Nomine Associates and others.
# All Rights Reserved.
# 
# This software has been released under the terms of the IBM Public
# License.  For details, see the LICENSE file in the top-level source
# directory or online at http://www.openafs.org/dl/license10.html
#

#
# this script is used to auto-generate tracepoint implementations
# from tracepoint table files
#

#
# by coding convention, local variables in function declarations
# are preceded by two spaces in order to differentiate them from
# call arguments
#

#
# trace framework definition file overview:
#
# the files parsed by this utility define all of the components
# of the instrumentation framework.
#
# the major components include: probes, statistics, and queries
#
# probes are further split into events and actions.  actions
# are a high-level construct to hierarchically organize many
# related probes (e.g. rpc calls have a start and finish event)
#
# statistics entries specify everything necessary to autogenerate
# stats storage structures, update routines, and associated 
# instrumentation.  In order to cut down on intrumentation namespace
# pollution somehwat, groups of statistics are allowed to share
# the same probe point name.  Groupings are generally used for
# closely related statistics (e.g. call count, sum of time in call,
# sum of squares of time in call, min call time, max call time, etc.)
#
# whereas probe points provide the "proactive" side to system
# monitoring, queries provide a form of "reactive" monitoring
# of system internals.  The purpose of defining them here is
# to allow for autogeneration of code related to the IPC subsystem.
# In addition, it allows us to autogenerate the code necessary to
# extract keys from a probe name template containing %d, %u, etc.
#

#
# tracepoint table definition file syntax:
#
# These files are constructed of 'key = value' pairs with nested scoping
#
# no key value pairs are acceptable at global scope.
#
# description scope
#  -- scope is legally nested anywhere.
#     anything (including further nested braces) is legal inside
#     description scope.  everything within description scope
#     is treated as a comment
#
# trace-table scope
#   -- only legal from file scope.  defines all instrumentation
#      features which are compiled into a single tracepoint module
# required key value pairs:
#   module-name
#     -- name of tracing module
#   module-dir
#     -- name of directory in which to generate source
#   module-version
#     -- version number for tracepoint definition file
#   c-path
#     -- name of c source file to generate
#   h-path
#     -- name of header file to generate
#   prefix
#     -- prefix for all probe point names (must be quoted)
#   init-func
#     -- name of c function which will initialize this trace table
#   event-tag
#     -- element from osi_Trace_EventType enumeration
# legal nested scopes:
#   description
#   event-table
#   action-table
#   stats-table
#   query-table
#
# event-table scope
#   -- only legal from trace-table scope.  defines a set of
#      events sharing a common probe name prefix
# required key value pairs:
#   name
#     -- prefix name
# optional key value pairs:
#   prefix
#     -- assign probe name different from C symbol name
# legal nested scopes:
#  description
#  event
#
# event scope
#   -- only legal from event-table or action scope
#

BEGIN {
    table = 0;
    tidx = 1;
    brace_depth = 0;

    obj_init();
    scope_init();
    parser_init();


    dpf("OpenAFS Instrumentation Framework");
    dpf("Automated Code Generator v0.1");
    dpf("Copyright (c) 2006-2007, Sine Nomine Associates and others.");
    dpf("parsing trace definition file " FILENAME "...");
}

#
# main parser
#

{
# handle brace depth calculations
    for (f=1; f <= NF; f++) {
	if ($f == "{") {
	    brace_depth++;
	} else if ($f == "}") {
	    brace_depth--;
	}
    }

    if (($0 == "") || ($1 == "#")) {

    } else if (scope() == scope_description) {
	if (brace_depth < desc_brace_depth) {
	    scope_pop();
	}
    } else if ($1 == "description") {
	if ($2 == "{") {
	    scope_push(scope_description);
	    desc_brace_depth = brace_depth;
	}
    } else if (scope() == scope_global) {
	if ($1 == "trace-table") {
	    event_table = 0;
	    action_table = 0;
	    stat_table = 0;
	    query_table = 0;
	    if ($2 == "{") {
		scope_push(scope_table);
	    } else {
		scope_push(scope_table_wait_brace);
	    }
	} else {
	    parse_panic("invalid token '" $1 "' at global scope");
	}
    } else if (scope() == scope_table_wait_brace) {
	if ($1 == "{") {
	    scope_current_replace(scope_table);
	} else {
	    parse_panic("expected token '{' missing after token 'trace-table'");
	}
    } else if (scope() == scope_table) {
	if ($1 == "}") {
	    event_tables[table] = event_table;
	    action_tables[table] = action_table;
	    stat_tables[table] = stat_table;
	    query_tables[table] = query_table;
	    table++;
	    scope_pop();
	} else if ($1 == "module-name") {
	    module_names[table] = $2;
	} else if ($1 == "module-dir") {
	    module_dirs[table] = $2;
	} else if ($1 == "module-version") {
	    module_versions[table] = $2;
	} else if ($1 == "c-path") {
	    module_c_paths[table] = $2;
	} else if ($1 == "h-path") {
	    module_h_paths[table] = $2;
	} else if ($1 == "prefix") {
	    module_prefixes[table] = $2;
	} else if ($1 == "init-func") {
	    module_init_funcs[table] = $2;
	} else if ($1 == "event-tag") {
	    module_events[table] = $2;
	} else if ($1 == "event-table") {
	    event = 0;
	    if ($2 == "{") {
		scope_push(scope_event_table);
	    } else {
		scope_push(scope_event_table_wait_brace);
	    }
	} else if ($1 == "action-table") {
	    action = 0;
	    if ($2 == "{") {
		scope_push(scope_action_table);
	    } else {
		scope_push(scope_action_table_wait_brace);
	    }
	} else if ($1 == "stats-table") {
	    stat_table_size[table,stat_table] = 0;
	    stat_table_probe_count[table,stat_table] = 0;
	    vstat = 0;
	    if ($2 == "{") {
		scope_push(scope_stat_table);
	    } else {
		scope_push(scope_stat_table_wait_brace);
	    }
	} else if ($1 == "query-table") {
	    query = 0;
	    if ($2 == "{") {
		scope_push(scope_query_table);
	    } else {
		scope_push(scope_query_table_wait_brace);
	    }
	} else {
	    parse_panic("unknown trace-table token '" $1 "'");
	}
    } else if (scope() == scope_event_table_wait_brace) {
	if ($1 == "{") {
	    scope_current_replace(scope_event_table);
	} else {
	    parse_panic("expected token '{' missing after token 'event-table'");
	}
    } else if (scope() == scope_event_table) {
	if ($1 == "}") {
	    if (event_table_name[table,event_table] == "") {
		parse_panic("required 'event-table' property 'name' not specified");
	    }
	    if (event_table_prefix[table,event_table] == "") {
		event_table_prefix[table,event_table] = \
		    "\"" event_table_name[table,event_table] "\"";
	    }
	    event_table_size[table,event_table] = event;
	    event_table++;
	    scope_pop();
	} else if ($1 == "name") {
	    event_table_name[table,event_table] = $2;
	} else if ($1 == "prefix") {
	    event_table_prefix[table,event_table] = $2;
	} else if ($1 == "event") {
	    if ($2 == "{") {
		scope_push(scope_event);
	    } else {
		scope_push(scope_event_wait_brace);
	    }
	} else {
	    parse_panic("unknown event-table token '" $1 "'");
	}
    } else if (scope() == scope_event_wait_brace) {
	if ($1 == "{") {
	    scope_current_replace(scope_event);
	} else {
	    parse_panic("expected token '{' missing after token 'event'");
	}
    } else if (scope() == scope_event) {
	if ($1 == "}") {
	    if (event_name[table,event_table,event] == "") {
		parse_panic("required 'event' property 'name' not specified");
	    }
	    if (event_suffix[table,event_table,event] == "") {
		event_suffix[table,event_table,event] = \
		    "\"" event_name[table,event_table,event] "\"";
	    }
	    event++;
	    scope_pop();
	} else if ($1 == "suffix") {
	    event_suffix[table,event_table,event] = $2;
	} else if ($1 == "name") {
	    event_name[table,event_table,event] = $2;
	} else if ($1 == "default-mode") {
	    event_default_mode[table,event_table,event] = $2;
	} else {
	    parse_panic("unknown event token '" $1 "'");
	}
    } else if (scope() == scope_action_table_wait_brace) {
	if ($1 == "{") {
	    scope_current_replace(scope_action_table);
	} else {
	    parse_panic("expected token '{' missing after token 'action-table'");
	}
    } else if (scope() == scope_action_table) {
	if ($1 == "}") {
	    if (action_table_name[table,action_table] == "") {
		parse_panic("required 'action-table' property 'name' not specified");
	    }
	    if (action_table_prefix[table,action_table] == "") {
		action_table_prefix[table,action_table] = \
		    "\"" action_table_name[table,action_table] "\"";
	    }
	    action_table_size[table,action_table] = action;
	    action_table++;
	    scope_pop();
	} else if ($1 == "name") {
	    action_table_name[table,action_table] = $2;
	} else if ($1 == "prefix") {
	    action_table_prefix[table,action_table] = $2;
	} else if ($1 == "action") {
	    action_event_count[table,action_table,action] = 0;
	    if ($2 == "{") {
		scope_push(scope_action);
	    } else {
		scope_push(scope_action_wait_brace);
	    }
	} else {
	    parse_panic("unknown action-table token '" $1 "'");
	}
    } else if (scope() == scope_action_wait_brace) {
	if ($1 == "{") {
	    scope_current_replace(scope_action);
	} else {
	    parse_panic("expected token '{' missing after token 'action'");
	}
    } else if (scope() == scope_action) {
	if ($1 == "}") {
	    if (action_name[table,action_table,action] == "") {
		parse_panic("required 'action' property 'name' not specified");
	    }
	    if (action_suffix[table,action_table,action] == "") {
		action_suffix[table,action_table,action] = \
		    "\"" action_name[table,action_table,action] "\"";
	    }
	    action++;
	    scope_pop();
	} else if ($1 == "suffix") {
	    action_suffix[table,action_table,action] = $2;
	} else if ($1 == "name") {
	    action_name[table,action_table,action] = $2;
	} else if ($1 == "event-default") {
	    action_event_default[table,action_table,action] = $2;
	} else if ($1 == "default-mode") {
	    action_default_mode[table,action_table,action] = $2;
	} else if ($1 == "event") {
	    if ($2 == "{") {
		scope_push(scope_action_event);
	    } else {
		scope_push(scope_action_event_wait_brace);
	    }
	} else {
	    parse_panic("unknown action token '" $1 "'");
	}
    } else if (scope() == scope_action_event_wait_brace) {
	if ($1 == "{") {
	    scope_current_replace(scope_action_event);
	} else {
	    parse_panic("expected token '{' missing after token 'event'");
	}
    } else if (scope() == scope_action_event) {
	if ($1 == "}") {
	    if (scope_local("name") == "") {
		parse_panic("required 'event' property 'name' not specified");
	    }
	    if (scope_local("suffix") == "") {
		scope_local("suffix",
			    "\"" scope_local("name") "\"");
	    }
	    if (scope_local("default-mode") == "") {
		if (action_default_mode[table,action_table,action]) {
		    scope_local("default-mode", 1);
		} else {
		    scope_local("default-mode", 0);
		}
	    }
	    add_action_event(table,action_table,action,
			     scope_local("name"),
			     scope_local("suffix"),
			     scope_local("default-mode"));
	    scope_pop();
	} else if (($1 == "name") || ($1 == "suffix") || ($1 == "default-mode")) {
	    scope_local($1,$2);
	} else {
	    parse_panic("unknown event token '" $1 "'");
	}
    } else if (scope() == scope_stat_table_wait_brace) {
	if ($1 == "{") {
	    scope_current_replace(scope_stat_table);
	} else {
	    parse_panic("expected token '{' missing after token 'stats-table'");
	}
    } else if (scope() == scope_stat_table) {
	if ($1 == "}") {
	    if (stat_table_name[table,stat_table] == "") {
		parse_panic("required 'stat-table' property 'name' not specified");
	    }
	    if (stat_table_type[table,stat_table] == "template") {
		stat_table_templates[stat_table_name[table,stat_table],scope_table] = table;
		stat_table_templates[stat_table_name[table,stat_table],scope_stat_table] = stat_table;
	    }
	    if (stat_table_prefix[table,stat_table] == "") {
		stat_table_prefix[table,stat_table] = \
		    "\"" stat_table_name[table,stat_table] "\"";
	    } else if (stat_table_prefix[table,stat_table] == "NULL") {
		stat_table_prefix[table,stat_table] = "";
	    }
	    stat_table++;
	    scope_pop();
	} else if ($1 == "name") {
	    stat_table_name[table,stat_table] = $2;
	} else if ($1 == "prefix") {
	    stat_table_prefix[table,stat_table] = $2;
	} else if ($1 == "type") {
	    if (($2 != "global") && ($2 != "context-local") && ($2 != "object-local") &&
		($2 != "template")) {
		parse_panic("invalid value '" $2 "' for 'stats-table' property 'type'");
	    }
	    stat_table_type[table,stat_table] = $2;
	} else if ($1 == "object-type") {
	    stat_table_object_type[table,stat_table] = $2;
	} else if ($1 == "object-member") {
	    stat_table_object_member[table,stat_table] = $2;
	} else if ($1 == "synchronization") {
	    stat_table_sync[table,stat_table] = $2;
	} else if ($1 == "storage-class") {
	    if (($2 != "static") && ($2 != "dynamic")) {
		parse_panic("invalid value '" $2 "' for 'stats-table' property 'storage-class'");
	    }
	    stat_table_storage_class[table,stat_table] = $2;
	} else if ($1 == "stat") {
	    if ($2 == "{") {
		scope_push(scope_stat);
	    } else {
		scope_push(scope_stat_wait_brace);
	    }
	} else if ($1 == "vstat") {
	    if ($2 == "{") {
		scope_push(scope_vstat);
	    } else {
		scope_push(scope_vstat_wait_brace);
	    }
	} else if ($1 == "group") {
	    if ($2 == "{") {
		scope_push(scope_stat_group);
	    } else {
		scope_push(scope_stat_group_wait_brace);
	    }
	} else if ($1 == "inherit") {
	    current_inherit_name = "";
	    current_inherit_suffix = "";
	    current_inherit_template = "";
	    if ($2 == "{") {
		scope_push(scope_stat_table_inherit);
	    } else {
		scope_push(scope_stat_table_inherit_wait_brace);
	    }
	} else {
	    parse_panic("unknown stats-table token '" $1 "'");
	}
    } else if (scope() == scope_stat_wait_brace) {
	if ($1 == "{") {
	    scope_current_replace(scope_stat);
	} else {
	    parse_panic("expected token '{' missing after token 'stat'");
	}
    } else if (scope() == scope_stat) {
	if ($1 == "}") {
	    if (scope_local("name") == "") {
		parse_panic("required 'stat' property 'name' not specified");
	    }
	    if (scope_local("type") == "") {
		parse_panic("required 'stat' property 'type' not specified");
	    }
	    if (scope_local("width") == "") {
		parse_panic("required 'stat' property 'width' not specified");
	    }
	    if (scope_local("suffix") == "") {
		scope_local("suffix",
			    "\"" scope_local("name") "\"");
	    }
	    if (scope_peek() == scope_stat_table) {
		p = add_stat_probe(table,stat_table,
				   scope_local("name"),
				   scope_local("suffix"),
				   scope_local("default-mode"));
		g = 0;
	    } else {
		if (scope_local("default-mode") != "") {
		    parse_panic("property 'default-mode' not allowed in 'stat' scope under 'group' scope");
		}
		p = stat_table_probe_count[table,stat_table];
		g = add_stat_group_member(table,stat_table,
					  p,
					  scope_local("name"),
					  scope_local("suffix"));
	    }
	    add_stat(table,stat_table,
		     p,
		     g,
		     scope_local("name"),
		     scope_local("suffix"),
		     scope_local("type"),
		     scope_local("subtype"),
		     scope_local("aggregator"),
		     scope_local("width"));
	    scope_pop();
	} else if (($1 == "name") || ($1 == "suffix") || ($1 == "type") ||
		   ($1 == "subtype") || ($1 == "aggregator") || 
		   ($1 == "width") || ($1 == "default-mode")) {
	    scope_local($1,$2);
	} else {
	    parse_panic("unknown stat token '" $1 "'");
	}
    } else if (scope() == scope_vstat_wait_brace) {
	if ($1 == "{") {
	    scope_current_replace(scope_vstat);
	} else {
	    parse_panic("expected token '{' missing after token 'vstat'");
	}
    } else if (scope() == scope_vstat) {
	if ($1 == "}") {
	    if (vstat_name[table,stat_table,vstat] == "") {
		parse_panic("required 'vstat' property 'name' not specified");
	    }
	    if (vstat_type[table,stat_table,vstat] == "") {
		parse_panic("required 'vstat' property 'type' not specified");
	    }
	    if (vstat_width[table,stat_table,vstat] == "") {
		parse_panic("required 'vstat' property 'width' not specified");
	    }
	    if (vstat_args[table,stat_table,vstat] == "") {
		parse_panic("required 'vstat' property 'equation' not specified");
	    }
	    if (vstat_suffix[table,stat_table,vstat] == "") {
		vstat_suffix[table,stat_table,vstat] = \
		    "\"" vstat_name[table,stat_table,vstat] "\"";
	    }
	    vstat++;
	    scope_pop();
	} else if ($1 == "name") {
	    vstat_name[table,stat_table,vstat] = $2;
	} else if ($1 == "suffix") {
	    vstat_suffix[table,stat_table,vstat] = $2;
	} else if ($1 == "type") {
	    vstat_type[table,stat_table,vstat] = $2;
	} else if ($1 == "subtype") {
	    vstat_subtype[table,stat_table,vstat] = $2;
	} else if ($1 == "width") {
	    vstat_width[table,stat_table,vstat] = $2;
	} else if ($1 == "equation") {
	    vstat_args[table,stat_table,vstat] = NF - 1;
	    for (a = 2; a < NF; a++) {
		vstat_arg_vec[table,stat_table,vstat,a] = $a;
	    }
	} else {
	    parse_panic("unknown vstat token '" $1 "'");
	}
    } else if (scope() == scope_stat_group_wait_brace) {
	if ($1 == "{") {
	    scope_current_replace(scope_stat_group);
	} else {
	    parse_panic("expected token '{' missing after token 'group'");
	}
    } else if (scope() == scope_stat_group) {
	if ($1 == "}") {
	    if (scope_local("name") == "") {
		parse_panic("required 'group' property 'name' not specified");
	    }
	    if (scope_local("suffix") == "") {
		scope_local("suffix",
			    "\"" scope_local("name") "\"");
	    }
	    add_stat_probe(table,stat_table,
			   scope_local("name"),
			   scope_local("suffix"),
			   scope_local("default-mode"));
	    scope_pop();
	} else if (($1 == "name") || ($1 == "suffix") || ($1 == "default-mode")) {
	    scope_local($1,$2);
	} else if ($1 == "stat") {
	    if ($2 == "{") {
		scope_push(scope_stat);
	    } else {
		scope_push(scope_stat_wait_brace);
	    }
	} else if ($1 == "inherit") {
	    if ($2 == "{") {
		scope_push(scope_stat_table_inherit);
	    } else {
		scope_push(scope_stat_table_inherit_wait_brace);
	    }
	} else {
	    parse_panic("unknown 'group' token '" $1 "'");
	}
    } else if (scope() == scope_stat_table_inherit_wait_brace) {
	if ($1 == "{") {
	    scope_current_replace(scope_stat_table_inherit);
	} else {
	    parse_panic("expected token '{' missing after token 'inherit'");
	}
    } else if (scope() == scope_stat_table_inherit) {
	if ($1 == "}") {
	    if (scope_local("name") == "") {
		parse_panic("required 'inherit' property 'name' not specified");
	    }
	    if (scope_local("template") == "") {
		parse_panic("required 'inherit' property 'template' not specified");
	    }
	    if (scope_local("suffix") == "") {
		scope_local("suffix",
		    "\"" scope_local("name") "\"");
	    }
	    inherit_stat_table(scope_local("name"),
			       scope_local("suffix"),
			       scope_local("template"));
	    scope_pop();
	} else if (($1 == "name") || ($1 == "suffix") || ($1 == "template")) {
	    scope_local($1,$2);
	} else {
	    parse_panic("unknown 'inherit' token '" $1 "'");
	}
    } else if (scope() == scope_query_table_wait_brace) {
	if ($1 == "{") {
	    scope_current_replace(scope_query_table);
	} else {
	    parse_panic("expected token '{' missing after token 'query-table'");
	}
    } else if (scope() == scope_query_table) {
	if ($1 == "}") {
	    if (query_table_name[table,query_table] == "") {
		parse_panic("required 'query-table' property 'name' not specified");
	    }
	    if (query_table_prefix[table,query_table] == "") {
		query_table_prefix[table,query_table] = \
		    "\"" query_table_name[table,query_table] "\"";
	    }
	    query_table_size[table,stat_table] = query;
	    query_table++;
	    scope_pop();
	} else if ($1 == "name") {
	    query_table_name[table,query_table] = $2;
	} else if ($1 == "prefix") {
	    query_table_prefix[table,query_table] = $2;
	} else if ($1 == "query") {
	    if ($2 == "{") {
		scope_push(scope_stat);
	    } else {
		scope_push(scope_stat_wait_brace);
	    }
	} else {
	    parse_panic("unknown query-table token '" $1 "'");
	}
    }
}

END {
    if (parse_error > 0) {
	exit(1);
    }
    if (scope() != scope_global) {
	panic("trace definition file missing closing brace(s)");
    }
    table_count = table;
    dpf("table_count = " table_count);

    postprocess();

    dpf("-=- c_gen loop -=-");
    for (t = 0; t < table_count; t++) {
	file = module_c_paths[t];
	dpf("t[" t "]: event_tables = " event_tables[t]);
	dpf("t[" t "]: action_tables = " action_tables[t]);
	dpf("t[" t "]: stat_tables = " stat_tables[t]);
	dpf("t[" t "]: query_tables = " query_tables[t]);
	dpf("t[" t "]: file = " file);
	copyright();
	c_top();
	c_gen(t);
	close(file);
    }
    dpf("-=- h_gen loop -=-");
    for (t = 0; t < table_count; t++) {
	file = module_h_paths[t];
	dpf("t[" t "]: event_tables = " event_tables[t]);
	dpf("t[" t "]: action_tables = " action_tables[t]);
	dpf("t[" t "]: stat_tables = " stat_tables[t]);
	dpf("t[" t "]: query_tables = " query_tables[t]);
	dpf("t[" t "]: file = " file);
	copyright();
	h_gen(t);
	close(file);
    }
}

function out(s) {
    print s > file;
}

function dpf(s) {
    print s;
}

function panic(s) {
    dpf("*** panic: " s);
    parse_error++;
    exit(1);
}

function parse_panic(s) {
    dpf("*** syntax error around " FILENAME ":" FNR " at scope type '" scope_name() "' ***");
    dpf("     " s);
    scope_dump();
    panic("parsing cannot continue");
}

#
# object and property api
#
# we implement "objects" as a multidimensional array
# dim 0 is the object identifier
# dim 1 is the property identifier
# dim 2 is an arbitrary index
#

# create a specific type of object
# add to the global object link table, and the type's link table
# index obj_create(type index)
function obj_create(type,  obj,obj_table) {
    obj = __obj_alloc();
    obj_prop_set(obj,prop_type,0,type);
    obj_prop_set(obj,prop_table_size,0,0);
    obj_prop_set(obj,prop_link_table,0,link_table_null);
    obj_link_table_add(link_table_all,obj);
    obj_table = obj_link_table_get(type);
    obj_link_table_add(obj_table,obj);
    return obj;
}

# create a specific type of object with a link table already attached
# add to the global object link table, and the type's link table
# index obj_create_container(type index)
function obj_create_container(type,  obj,obj_table) {
    obj = obj_create(type);
    obj_table = obj_link_table_create();
    obj_link_table_set(obj,obj_table);
    return obj;
}

# low-level interface to create a specific type of object:
# index __obj_create(type index)
function __obj_create(type,  obj) {
    obj = __obj_alloc();
    obj_prop_set(obj,prop_type,0,type);
    obj_prop_set(obj,prop_table_size,0,0);
    return obj;
}

# low-level interface to allocate an object
# object __obj_alloc(void)
function __obj_alloc() {
    return obj_prop_inc(obj_meta,prop_table_size,0);
}

# set an object property:
# void obj_prop_set(object, property, property index, value)
function obj_prop_set(obj,prop,idx,val) {
    objects[obj,prop,idx] = val;
}

# get an object property:
# val obj_prop_get(object, property, property index)
function obj_prop_get(obj,prop,idx) {
    return objects[obj,prop,idx];
}

# increment and return old value for property
# val obj_prop_inc(object, property, property index)
function obj_prop_inc(obj,prop,idx) {
    return objects[obj,prop,idx]++;
}

# get the object type
# type obj_type(object)
function obj_type(obj) {
    return obj_prop_get(obj,prop_type,0);
}

# register a new object type:
# object_index obj_register(name)
function obj_register(name,  obj) {
    obj = obj_create_container(obj_meta);
    obj_prop_set(obj,prop_name,0,name);
    return obj;
}

# prop_index prop_register(name)
function prop_register(name,  obj) {
    obj = obj_create(obj_prop);
    obj_prop_set(obj,prop_name,0,name);
    return obj;
}

# initialize object and property system
# we have to specially bootstrap obj_meta and obj_prop
# and several core object properties
# void obj_init(void)
function obj_init(  obj_table,idx) {
    idx=0;
    obj_null = idx++;
    obj_meta = idx++;
    prop_type = idx++;
    prop_table_size = idx++;
    prop_name = idx++;
    obj_prop_set(obj_meta,prop_type,0,obj_meta);
    obj_prop_set(obj_meta,prop_table_size,0,idx);
    obj_prop_set(obj_meta,prop_name,0,"object");

    obj_prop = __obj_create(obj_meta);
    obj_prop_set(obj_prop,prop_name,0,"object property");

    obj_prop_set(prop_type,prop_type,0,obj_prop);
    obj_prop_set(prop_table_size,prop_type,0,obj_prop);
    obj_prop_set(prop_name,prop_type,0,obj_prop);

    obj_prop_set(prop_type,prop_table_size,0,0);
    obj_prop_set(prop_table_size,prop_table_size,0,0);
    obj_prop_set(prop_name,prop_table_size,0,0);

    obj_prop_set(prop_type,prop_name,0,"object type");
    obj_prop_set(prop_table_size,prop_name,0,"table size");
    obj_prop_set(prop_name,prop_name,0,"name");

    obj_link_init();
}

#
# object linking api
# (basically a linked list api implemented as an array)
#

# create a link table object
# table obj_link_table_create(void)
function obj_link_table_create() {
    return obj_create(obj_link_table);
}

# add a link to an existing link table
# void obj_link_table_add(table, object)
function obj_link_table_add(table,obj,  idx) {
    if (table == link_table_null) {
	panic("tried to add object " obj " to the null link table");
    }
    idx = obj_prop_inc(table, prop_table_size, 0);
    obj_prop_set(table, prop_link, idx, obj);
}

# int obj_link_table_size(table)
function obj_link_table_size(table) {
    return obj_prop_get(table, prop_table_size, 0);
}

# obj obj_link_table_lookup_row(table, row index)
function obj_link_table_lookup_row(table,idx) {
    return obj_prop_get(table, prop_link, idx);
}

# obj obj_link_table_walk(table, iterator)
function obj_link_table_walk(table,iter,  idx) {
    idx = iter[0]++;
    if (idx < obj_link_table_size(table)) {
	return obj_link_table_lookup_row(table,idx);
    } else {
	return obj_null;
    }
}

# attach a link table to an object 
# (in theory link tables can be simultaneously referenced by multiple objects)
# void obj_link_table_set(object, link table)
function obj_link_table_set(obj,table) {
    obj_prop_set(obj, prop_link_table, 0, table);
}

# get the link table currently attached to an object
# table obj_link_table_get(object)
function obj_link_table_get(obj) {
    return obj_prop_get(obj, prop_link_table, 0);
}

# link one object to another using the parent's link table
# void obj_link(parent object, child object)
function obj_link(parent,child,  table) {
    table = obj_prop_get(parent, prop_link_table, 0);
    if (table == link_table_null) {
	panic("tried to make object " child " a child of non-container object " parent);
    }
    obj_link_table_add(table, child);
}

# walk a container object's children
# obj obj_walk_children(obj, iterator)
function obj_walk_children(obj,iter,  table) {
    table = obj_link_table_get(obj);
    if (table == link_table_null) {
	panic("tried to walk children of a non-container object " obj);
    }
    return obj_link_table_walk(table, iter);
}

# walk a container object's children of a certain type
# obj obj_walk_children_type(obj, type, iterator)
function obj_walk_children_type(parent,type,iter,  table,child) {
    table = obj_link_table_get(parent);
    if (table == link_table_null) {
	panic("tried to walk children of a non-container object " parent);
    }
    do {
	child = obj_link_table_walk(table, iter);
    } while ((child != obj_null) && (type != obj_type(child)));
    return child;
}

# bootstrap a container object
# void __obj_link_bootstrap_container(object)
function __obj_link_bootstrap_container(obj,  table) {
    table = __obj_create(obj_link_table);
    obj_link_table_set(table,link_table_null);
    obj_link_table_set(obj,table);
    obj_link_table_add(link_table_all,obj);
    obj_link_table_add(link_table_all,table);
    obj_link(obj_link_table,table);
}

# bootstrap a leaf object
# void __obj_link_bootstrap_leaf(object)
function __obj_link_bootstrap_leaf(obj,  table) {
    obj_link_table_set(obj, link_table_null);
    obj_link_table_add(link_table_all, obj);
}

# initialize object linking
# bootstrap linking into the various low-level objects
# void obj_link_init(void)
function obj_link_init() {
    obj_link_table = __obj_create(obj_meta);
    obj_prop_set(obj_link_table,prop_name,0,"link table");

    prop_link_table = __obj_create(obj_prop);
    obj_prop_set(prop_link_table,prop_name,0,"link table");
    prop_link = __obj_create(obj_prop);
    obj_prop_set(prop_link,prop_name,0,"link");

    link_table_all = __obj_create(obj_link_table);
    link_table_null = __obj_create(obj_link_table);

    __obj_link_bootstrap_leaf(obj_null);
    __obj_link_bootstrap_leaf(link_table_all);
    __obj_link_bootstrap_leaf(link_table_null);
    __obj_link_bootstrap_leaf(prop_type);
    __obj_link_bootstrap_leaf(prop_table_size);
    __obj_link_bootstrap_leaf(prop_name);
    __obj_link_bootstrap_leaf(prop_link_table);
    __obj_link_bootstrap_leaf(prop_link);

    __obj_link_bootstrap_container(obj_link_table);
    __obj_link_bootstrap_container(obj_meta);
    __obj_link_bootstrap_container(obj_prop);

    obj_link(obj_meta,obj_meta);
    obj_link(obj_meta,obj_prop);
    obj_link(obj_meta,obj_link_table);

    obj_link(obj_prop,prop_type);
    obj_link(obj_prop,prop_table_size);
    obj_link(obj_prop,prop_name);
    obj_link(obj_prop,prop_link_table);
    obj_link(obj_prop,prop_link);

    obj_link(obj_link_table,link_table_all);
    obj_link(obj_link_table,link_table_null);
}

#
# parser scoping control
#

# scope_object scope_create(scope type)
function scope_create(scope,  obj) {
    obj = obj_create(obj_scope);
    obj_prop_set(obj,prop_scope,0,scope);
    return obj;
}

# scope_index scope_register(name)
function scope_register(name,  obj) {
    obj = obj_create(obj_scope_meta);
    obj_prop_set(obj,prop_name,0,name);
    obj_prop_set(obj,prop_parse_meta_obj,0,obj_null);
    return obj;
}

# void scope_parse_meta_set(scope meta object, parser metadata object)
function scope_parse_meta_set(scope,parse) {
    obj_prop_set(scope,prop_parse_meta_obj,0,parse);
}

# parse meta object scope_parse_meta_get(scope meta object)
function scope_parse_meta_get(scope) {
    return obj_prop_get(scope,prop_parse_meta_obj,0);
}

# parse meta object scope_parse_meta_get_current(void)
function scope_parse_meta_get_current() {
    return scope_parse_meta_get(scope());
}

# this method returns the type object with the current scope
# scope type object scope(void)
function scope() {
    return _scope_type;
}

# scope name string scope_name(void)
function scope_name() {
    return obj_prop_get(_scope_type,prop_name,0);
}

# this method returns the scope-local object allocated for this particular scope instance
# scope object scope_obj(void)
function scope_obj() {
    return _scope_obj;
}

# void scope_reset(new_scope)
function scope_reset(new_scope) {
    scope_stack_depth = 0;
    scope_push(new_scope);
}

# old_scope scope_push(new scope meta object)
function scope_push(new_scope,  obj) {
    obj = scope_create(new_scope);

    scope_stack[++scope_stack_depth] = obj;
    _scope_type = new_scope;
    _scope_obj = obj;

    scope_local_push();

    return save;
}

# restore_scope scope_pop(void)
function scope_pop() {
    scope_local_pop();
    _scope_obj = scope_stack[--scope_stack_depth];
    _scope_type = obj_prop_get(_scope_obj,prop_scope,0);
    return _scope_type;
}

# parent_scope scope_peek(void)
function scope_peek(  obj) {
    obj = scope_stack[scope_stack_depth-1];
    return obj_prop_get(obj,prop_scope,0);
}

# void scope_current_replace(scope meta object)
function scope_current_replace(new_scope) {
    _scope_type = new_scope;
    obj_prop_set(_scope_obj,prop_scope,0,new_scope);
}

# void scope_dump(void)
function scope_dump(  s,obj) {
    dpf("*** begin scope stack dump ***");
    for (s = 1; s <= scope_stack_depth; s++) {
	obj = scope_stack[s];
	dpf("     s[" s "]: " obj_prop_get(obj,prop_scope,0));
    }
    dpf("*** end scope stack dump ***");
}

# current_value scope_local(key, new_value)
function scope_local(key,new_val,  save_val)
{
    save_val = scope_local_data[scope_stack_depth,key];
    if (new_val != "") {
	scope_local_data[scope_stack_depth,key] = new_val;
    }
    if (scope_local_key_idx[scope_stack_depth,key] == "") {
	scope_local_key_reg(key);
    }
    return save_val;
}

# current_value scope_local_unset(key)
function scope_local_unset(key,  save_val)
{
    save_val = scope_local(key);
    scope_local_data[scope_stack_depth,key] = "";
    return save_val;
}

# void scope_local_key_reg(key)
function scope_local_key_reg(key) {
    scope_local_key_dir[scope_stack_depth,scope_local_keys[scope_stack_depth]] = key;
    scope_local_key_idx[scope_stack_depth,key] = scope_local_keys[scope_stack_depth]++;
}

# void scope_local_pop(void)
function scope_local_pop(  k,key) {
    for (k = 0; k < scope_local_keys[scope_stack_depth]; k++) {
	key = scope_local_key_dir[scope_stack_depth,k];
	scope_local_data[scope_stack_depth,key] = "";
	scope_local_key_idx[scope_stack_depth,key] = "";
	scope_local_key_dir[scope_stack_depth,k] = "";
    }
}

# void scope_local_push(void)
function scope_local_push() {
    scope_local_keys[scope_stack_depth] = 0;
}

# initialize scope package
# void scope_init(void)
function scope_init() {
    obj_scope = obj_register("scope");
    obj_scope_meta = obj_register("scope metadata");

    prop_scope = prop_register("scope");
    prop_parse_meta_obj = prop_register("parser metadata object");

    scope_global = scope_register("global");
    scope_table_wait_brace = scope_register("table_wait_brace");
    scope_table = scope_register("table");
    scope_event_table_wait_brace = scope_register("event_table_wait_brace");
    scope_event_table = scope_register("event_table");
    scope_action_table_wait_brace = scope_register("action_table_wait_brace");
    scope_action_table = scope_register("action_table");
    scope_stat_table_wait_brace = scope_register("stat_table_wait_brace");
    scope_stat_table = scope_register("stat_table");
    scope_query_table_wait_brace = scope_register("query_table_wait_brace");
    scope_query_table = scope_register("query_table");
    scope_event_wait_brace = scope_register("event_wait_brace");
    scope_event = scope_register("event");
    scope_action_wait_brace = scope_register("action_wait_brace");
    scope_action = scope_register("action");
    scope_action_event_wait_brace = scope_register("action_event_wait_brace");
    scope_action_event = scope_register("action_event");
    scope_stat_group_wait_brace = scope_register("stat_group_wait_brace");
    scope_stat_group = scope_register("stat_group");
    scope_stat_wait_brace = scope_register("stat_wait_brace");
    scope_stat = scope_register("stat");
    scope_vstat_wait_brace = scope_register("vstat_wait_brace");
    scope_vstat = scope_register("vstat");
    scope_stat_table_inherit_wait_brace = scope_register("stat_table_inherit_wait_brace");
    scope_stat_table_inherit = scope_register("stat_table_inherit");
    scope_query_wait_brace = scope_register("query_wait_brace");
    scope_query = scope_register("query");
    scope_description = scope_register("description");

    scope_reset(scope_global);
}

#
# postprocessor support
#

# void postprocess(void)
function postprocess(  t) {
    for (t = 0; t < table_count; t++) {
	postprocess_table(t);
    }
}

# void postprocess_table(table index)
function postprocess_table(t,  at,et,st,qt) {
    for (at = 0; at < action_tables[t]; at++) {
	postprocess_action_table(t,at);
    }
}

# void postprocess_action_table(table index, action table index)
function postprocess_action_table(t,at,  a) {
    for (a = 0; a < action_table_size[t,at]; a++) {
	postprocess_action_events(t,at,a);
    }
}

# void postprocess_action_events(table index, action table index, action index)
function postprocess_action_events(t,at,a) {
    if (action_event_default[t,at,a] != "no") {
	add_action_event(t,at,a,"start","\"start\"",action_default_mode[t,at,a]);
	add_action_event(t,at,a,"finish","\"finish\"",action_default_mode[t,at,a]);
	add_action_event(t,at,a,"abort","\"abort\"",action_default_mode[t,at,a]);
    }
}

#
# parser support
#

# parser initialization
# void parser_init(void)
function parser_init() {
    parse_object_init();
    trace_table_init();
    action_table_init();
    event_table_init();
    stats_table_init();
    query_table_init();
    action_init();
    event_init();
    stat_group_init();
    stat_init();
    vstat_init();
    query_init();
}

#
# master parse object
#

# void parse_object_init(void)
function parse_object_init(  table) {
    obj_parser = obj_register("parser");
    obj_parse_meta = obj_register("parser metadata");
    obj_parse_key_meta = obj_register("parser key metadata");
    obj_parse_key = obj_register("parser key-value pair");

    parser = obj_create_container(obj_parser);

    parse_meta_init();
    parse_meta_key_init();
}

#
# parse object metadata
#

# obj parse_meta_create(object name)
function parse_meta_create(name,  obj) {
    obj = obj_create_container(obj_parse_meta);
    obj_prop_set(obj,prop_name,0,name);
    return obj;
}

# void parse_meta_register(parse meta parent, parse meta child)
function parse_meta_register(parent,child) {
    obj_link(parent,child);
}

# void parse_meta_init(void)
function parse_meta_init() {
    parse_meta_global = obj_create_container(obj_parse_meta);
}

#
# parse key metadata
#

# obj parse_meta_key_create(key name, int required)
function parse_meta_key_create(name,req,  obj) {
    obj = obj_create_container(obj_parse_key_meta);
    obj_prop_set(obj,prop_key,0,name);
    obj_prop_set(obj,prop_required,0,req);
    return obj;
}

# void parse_meta_key_default_set(key object, default value)
function parse_meta_key_default_set(key, val) {
    obj_prop_set(obj,prop_default_val,0,val);
}

# default value parse_meta_key_default_get(key object)
function parse_meta_key_default_get(key) {
    return obj_prop_get(obj,prop_default_val,0);
}

# set values for an enumeration
# void parse_meta_key_enum_add(key object, allowed value)
function parse_meta_key_enum_add(key,val,  obj) {
    obj = obj_create(obj_parse_key_enum_meta);
    obj_prop_set(obj,prop_name,0,val);
    obj_link(key, obj);
}

# validate an value against an enumeration definition
# int parse_meta_key_enum_validate(key object, value)
function parse_meta_key_enum_validate(key,val,  iter,obj) {
    obj = obj_walk_children_type(key, obj_parse_key_enum_meta, iter);
    while (obj != obj_null) {
	if (val == obj_prop_get(obj,prop_name,0)) {
	    return 1;
	}
	obj = obj_walk_children_type(key, obj_parse_key_enum_meta, iter);
    }
    return 0;
}

# lookup parse meta object or parse meta key object which matches
# the passed in key for the passed in parse meta obj
# object parse_meta_key_lookup(meta obj, key string)
function parse_meta_key_lookup(meta_obj,key,  child,iter) {
    child = obj_walk_children(meta_obj,iter);
    while (child != obj_null) {
	if ((obj_parse_meta == obj_type(child)) &&
	    (key == obj_prop_get(child,prop_name,0))) {
	    return child;
	}
	if ((obj_parse_key_meta == obj_type(child)) &&
	    (key == obj_prop_get(child,prop_key,0))) {
	    return child;
	}
	child = obj_walk_children(meta_obj,iter);
    }
    return obj_null;
}

# lookup parse meta object or parse meta key object which matches
# the passed in key *for the current scope*
# object parse_meta_key_lookup_current(key string)
function parse_meta_key_lookup_current(key,  meta_obj,child,iter) {
    meta_obj = scope_parse_meta_get_current();
    if (meta_obj == obj_null) {
	panic("no parse metadata object registered for parse scope type '" scope_name() "'");
    }
    return parse_meta_key_lookup(meta_obj,key);
}

# void parse_meta_key_register(parse meta object, parse key meta object)
function parse_meta_key_register(obj,key_obj) {
    obj_link(obj, key_obj);
}

# void parse_meta_key_init(void)
function parse_meta_key_init() {
    obj_parse_key_enum_meta = obj_register("key enumeration values metadata");

    prop_key = prop_register("key");
    prop_required = prop_register("required");
    prop_default_val = prop_register("default value");
}

#
# trace table object
#

# table_index trace_table_create(void)
function trace_table_create() {
    return obj_create_container(obj_trace_table);
}

# void trace_table_register(parser object, trace table object)
function trace_table_register(parse,table) {
    obj_link(parse,table);
}

# void trace_table_init(void)
function trace_table_init(  key) {
    obj_trace_table = obj_register("trace_table");

    parse_meta_table = parse_meta_create("trace-table");

    key = parse_meta_key_create("module-name", 1);
    parse_meta_key_register(parse_meta_table, key);

    key = parse_meta_key_create("module-dir", 1);
    parse_meta_key_register(parse_meta_table, key);

    key = parse_meta_key_create("module-version", 1);
    parse_meta_key_register(parse_meta_table, key);

    key = parse_meta_key_create("c-path", 1);
    parse_meta_key_register(parse_meta_table, key);

    key = parse_meta_key_create("h-path", 1);
    parse_meta_key_register(parse_meta_table, key);

    key = parse_meta_key_create("prefix", 0);
    parse_meta_key_register(parse_meta_table, key);

    key = parse_meta_key_create("init-func", 1);
    parse_meta_key_register(parse_meta_table, key);

    key = parse_meta_key_create("event-tag", 1);
    parse_meta_key_register(parse_meta_table, key);

    key = parse_meta_key_create("description", 0);
    parse_meta_key_register(parse_meta_table, key);

    parse_meta_register(parse_meta_global, parse_meta_table);
    scope_parse_meta_set(scope_table, parse_meta_table);
}

#
# action table object
#

# table_index action_table_create(void)
function action_table_create() {
    return obj_create_container(obj_action_table);
}

# void action_table_register(trace table object, action table object)
function action_table_register(table,action_table) {
    obj_link(table, action_table);
}

# void action_table_init(void)
function action_table_init(  key) {
    obj_action_table = obj_register("action_table");

    parse_meta_action_table = parse_meta_create("action-table");

    key = parse_meta_key_create("name", 1);
    parse_meta_key_register(parse_meta_action_table, key);

    key = parse_meta_key_create("prefix", 0);
    parse_meta_key_register(parse_meta_action_table, key);

    key = parse_meta_key_create("description", 0);
    parse_meta_key_register(parse_meta_action_table, key);

    parse_meta_register(parse_meta_table, parse_meta_action_table);
    scope_parse_meta_set(scope_action_table, parse_meta_action_table);
}

#
# event table object
#

# table_index event_table_create(void)
function event_table_create() {
    return obj_create_container(obj_event_table);
}

# void event_table_register(trace table object, event table object)
function event_table_register(table,event_table) {
    obj_link(table, event_table);
}

# void event_table_init(void)
function event_table_init(  key) {
    obj_event_table = obj_register("event_table");

    parse_meta_event_table = parse_meta_create("event-table");

    key = parse_meta_key_create("name", 1);
    parse_meta_key_register(parse_meta_event_table, key);

    key = parse_meta_key_create("prefix", 0);
    parse_meta_key_register(parse_meta_event_table, key);

    key = parse_meta_key_create("description", 0);
    parse_meta_key_register(parse_meta_event_table, key);

    parse_meta_register(parse_meta_table, parse_meta_event_table);
    scope_parse_meta_set(scope_event_table, parse_meta_event_table);
}

#
# stats table object
#

# table_index stats_table_create(void)
function stats_table_create() {
    return obj_create_container(obj_stats_table);
}

# void stats_table_register(trace table object, stats table object)
function stats_table_register(table,stats_table) {
    obj_link(table, stats_table);
}

# void stats_table_init(void)
function stats_table_init(  key) {
    obj_stats_table = obj_register("stats_table");

    parse_meta_stat_table = parse_meta_create("stats-table");

    key = parse_meta_key_create("name", 1);
    parse_meta_key_register(parse_meta_stat_table, key);

    key = parse_meta_key_create("prefix", 0);
    parse_meta_key_register(parse_meta_stat_table, key);

    key = parse_meta_key_create("type", 1);
    parse_meta_key_enum_add(key, "global");
    parse_meta_key_enum_add(key, "object-local");
    parse_meta_key_enum_add(key, "context-local");
    parse_meta_key_register(parse_meta_stat_table, key);

    key = parse_meta_key_create("object-type", 0);
    parse_meta_key_register(parse_meta_stat_table, key);

    key = parse_meta_key_create("object-member", 0);
    parse_meta_key_register(parse_meta_stat_table, key);

    key = parse_meta_key_create("storage-class", 0);
    parse_meta_key_enum_add(key, "static");
    parse_meta_key_enum_add(key, "dynamic");
    parse_meta_key_register(parse_meta_stat_table, key);

    key = parse_meta_key_create("synchronization", 1);
    parse_meta_key_enum_add(key, "implicit");
    parse_meta_key_enum_add(key, "explicit"); 
    parse_meta_key_enum_add(key, "external");
    parse_meta_key_enum_add(key, "context-local");
    parse_meta_key_register(parse_meta_stat_table, key);

    key = parse_meta_key_create("description", 0);
    parse_meta_key_register(parse_meta_stat_table, key);

    parse_meta_register(parse_meta_table, parse_meta_stat_table);
    scope_parse_meta_set(scope_stat_table, parse_meta_stat_table);
}

#
# query table object
#

# table_index query_table_create(void)
function query_table_create() {
    return obj_create_container(obj_query_table);
}

# void query_table_register(trace table object, query table object)
function query_table_register(table,query_table) {
    obj_link(table, query_table);
}

# void query_table_init(void)
function query_table_init(  key) {
    obj_query_table = obj_register("query_table");

    parse_meta_query_table = parse_meta_create("query-table");

    key = parse_meta_key_create("name", 1);
    parse_meta_key_register(parse_meta_query_table, key);

    key = parse_meta_key_create("prefix", 0);
    parse_meta_key_register(parse_meta_query_table, key);

    key = parse_meta_key_create("index-dims", 1);
    parse_meta_key_register(parse_meta_query_table, key);

    key = parse_meta_key_create("description", 0);
    parse_meta_key_register(parse_meta_query_table, key);

    parse_meta_register(parse_meta_table, parse_meta_query_table);
    scope_parse_meta_set(scope_query_table, parse_meta_query_table);
}

#
# action object
# 

# action_index action_create(void)
function action_create() {
    return obj_create_container(obj_action);
}

# void action_register(action table object, action object)
function action_register(action_table,action) {
    obj_link(action_table, action);
}

# void action_init(void)
function action_init(  key) {
    obj_action = obj_register("action");

    parse_meta_action = parse_meta_create("action");

    key = parse_meta_key_create("name", 1);
    parse_meta_key_register(parse_meta_action, key);

    key = parse_meta_key_create("prefix", 0);
    parse_meta_key_register(parse_meta_action, key);

    key = parse_meta_key_create("event-default", 0);
    parse_meta_key_enum_add(key, "yes");
    parse_meta_key_enum_add(key, "no");
    parse_meta_key_default_set(key, "yes");
    parse_meta_key_register(parse_meta_action, key);

    key = parse_meta_key_create("description", 0);
    parse_meta_key_register(parse_meta_action, key);

    parse_meta_register(parse_meta_action_table, parse_meta_action);
    scope_parse_meta_set(scope_action, parse_meta_action);
}

#
# event object
# 

# event_index event_create(void)
function event_create() {
    return obj_create(obj_event);
}

# void event_register(event or action table object, event object)
function event_register(table,event) {
    obj_link(table, event);
}

# void event_init(void)
function event_init() {
    obj_event = obj_register("event");

    parse_meta_event = parse_meta_create("event");

    key = parse_meta_key_create("name", 1);
    parse_meta_key_register(parse_meta_event, key);

    key = parse_meta_key_create("prefix", 0);
    parse_meta_key_register(parse_meta_event, key);

    key = parse_meta_key_create("description", 0);
    parse_meta_key_register(parse_meta_event, key);

    parse_meta_register(parse_meta_event_table, parse_meta_event);
    parse_meta_register(parse_meta_action, parse_meta_event);
    scope_parse_meta_set(scope_event, parse_meta_event);
    scope_parse_meta_set(scope_action_event, parse_meta_event);
}

#
# stat group object
#

# stat_group stat_group_create(void)
function stat_group_create() {
    return obj_create(obj_stat_group);
}

# void stat_group_register(stat table object, stat group object)
function stat_group_register(stat_table,stat_group) {
    obj_link(stat_table,stat_group);
}

# void stat_group_init(void)
function stat_group_init() {
    obj_stat_group = obj_register("stat-group");

    parse_meta_stat_group = parse_meta_create("stat-group");

    key = parse_meta_key_create("name", 1);
    parse_meta_key_register(parse_meta_stat_group, key);

    key = parse_meta_key_create("prefix", 0);
    parse_meta_key_register(parse_meta_stat_group, key);

    key = parse_meta_key_create("template", 0);
    parse_meta_key_enum_add(parse_meta_stat_group, "no");
    parse_meta_key_enum_add(parse_meta_stat_group, "yes");
    parse_meta_key_default_set(parse_meta_stat_group, "no");
    parse_meta_key_register(parse_meta_stat_group, key);

    key = parse_meta_key_create("description", 0);
    parse_meta_key_register(parse_meta_stat_group, key);

    parse_meta_register(parse_meta_stat_table, parse_meta_stat_group);
    scope_parse_meta_set(scope_stat_group, parse_meta_stat_group);
}

#
# stat object
# 

# stat_index stat_create(void)
function stat_create() {
    return obj_create(obj_stat);
}

# void stat_register(stat table object, stat object)
function stat_register(stat_table,stat) {
    obj_link(stat_table, stat);
}

# void stat_init(void)
function stat_init() {
    obj_stat = obj_register("stat");

    parse_meta_stat = parse_meta_create("stat");

    key = parse_meta_key_create("name", 1);
    parse_meta_key_register(parse_meta_stat, key);

    key = parse_meta_key_create("suffix", 0);
    parse_meta_key_register(parse_meta_stat, key);

    key = parse_meta_key_create("type", 1);
    parse_meta_key_enum_add(key, "integer");
    parse_meta_key_enum_add(key, "time");
    parse_meta_key_register(parse_meta_stat, key);

    key = parse_meta_key_create("subtype", 0);
    parse_meta_key_enum_add(key, "elapsed");
    parse_meta_key_register(parse_meta_stat, key);

    key = parse_meta_key_create("aggregator", 1);
    parse_meta_key_enum_add(key, "counter");
    parse_meta_key_enum_add(key, "last");
    parse_meta_key_enum_add(key, "sum");
    parse_meta_key_enum_add(key, "min");
    parse_meta_key_enum_add(key, "max");
    parse_meta_key_enum_add(key, "sumOfSquare");
    parse_meta_key_register(parse_meta_stat, key);

    key = parse_meta_key_create("width", 1);
    parse_meta_key_enum_add(key, 8);
    parse_meta_key_enum_add(key, 16);
    parse_meta_key_enum_add(key, 32);
    parse_meta_key_enum_add(key, 64);
    parse_meta_key_register(parse_meta_stat, key);

    key = parse_meta_key_create("template", 0);
    parse_meta_key_enum_add(parse_meta_stat, "no");
    parse_meta_key_enum_add(parse_meta_stat, "yes");
    parse_meta_key_default_set(parse_meta_stat, "no");
    parse_meta_key_register(parse_meta_stat, key);

    key = parse_meta_key_create("description", 0);
    parse_meta_key_register(parse_meta_stat, key);

    parse_meta_register(parse_meta_stat_table, parse_meta_stat);
    parse_meta_register(parse_meta_stat_group, parse_meta_stat);
    scope_parse_meta_set(scope_stat, parse_meta_stat);
}

#
# vstat object
# 

# vstat_index vstat_create(void)
function vstat_create() {
    return obj_create(obj_vstat);
}

# void vstat_register(stat table object, vstat object)
function vstat_register(stat_table,vstat) {
    obj_link(stat_table, vstat);
}

# void vstat_init(void)
function vstat_init() {
    obj_vstat = obj_register("vstat");

    parse_meta_vstat = parse_meta_create("vstat");

    key = parse_meta_key_create("name", 1);
    parse_meta_key_register(parse_meta_vstat, key);

    key = parse_meta_key_create("suffix", 0);
    parse_meta_key_register(parse_meta_vstat, key);

    key = parse_meta_key_create("type", 1);
    parse_meta_key_enum_add(key, "integer");
    parse_meta_key_enum_add(key, "time");
    parse_meta_key_register(parse_meta_vstat, key);

    key = parse_meta_key_create("subtype", 0);
    parse_meta_key_enum_add(key, "elapsed");
    parse_meta_key_register(parse_meta_vstat, key);

    key = parse_meta_key_create("width", 1);
    parse_meta_key_enum_add(key, 8);
    parse_meta_key_enum_add(key, 16);
    parse_meta_key_enum_add(key, 32);
    parse_meta_key_enum_add(key, 64);
    parse_meta_key_register(parse_meta_vstat, key);

    key = parse_meta_key_create("equation", 1);
    parse_meta_key_register(parse_meta_vstat, key);

    key = parse_meta_key_create("template", 0);
    parse_meta_key_enum_add(parse_meta_vstat, "no");
    parse_meta_key_enum_add(parse_meta_vstat, "yes");
    parse_meta_key_default_set(parse_meta_vstat, "no");
    parse_meta_key_register(parse_meta_vstat, key);

    key = parse_meta_key_create("description", 0);
    parse_meta_key_register(parse_meta_vstat, key);

    parse_meta_register(parse_meta_stat_table, parse_meta_vstat);
    parse_meta_register(parse_meta_stat_group, parse_meta_vstat);
    scope_parse_meta_set(scope_vstat, parse_meta_vstat);
}

#
# query object
# 

# query_index query_create(void)
function query_create() {
    return obj_create(obj_query);
}

# void query_register(query table object, query object)
function query_register(query_table,query) {
    obj_link(query_table, query);
}

# void query_init(void)
function query_init() {
    obj_query = obj_register("query");

    parse_meta_query = parse_meta_create("query");

    key = parse_meta_key_create("name", 1);
    parse_meta_key_register(parse_meta_query, key);

    key = parse_meta_key_create("prefix", 0);
    parse_meta_key_register(parse_meta_query, key);

    key = parse_meta_key_create("description", 0);
    parse_meta_key_register(parse_meta_query, key);

    parse_meta_register(parse_meta_query_table, parse_meta_query);
    scope_parse_meta_set(scope_query, parse_meta_query);
}

#
# stat table templating support
#

# void inherit_stat_table(name,suffix,template)
function inherit_stat_table(name,suffix,template,  t,st,tt,tst,ts,tp,pmap,g) {
    tt = stat_table_templates[template,scope_table];
    tst = stat_table_templates[template,scope_stat_table];

    if ((tt == "") || (tst == "")) {
	parse_panic("invalid 'stat-table' template '" template "' referenced");
    }

    t = table;
    st = stat_table;

    for (tp = 0; tp < stat_table_probe_count[tt,tst]; tp++) {
	pmap[tp] = add_stat_probe(t,st,
				  name "_" stat_probe_name[tt,tst,tp],
				  suffix "\".\"" stat_probe_suffix[tt,tst,tp],
				  stat_probe_default_mode[tt,tst,tp]);
    }

    for (ts = 0; ts < stat_table_size[tt,tst]; ts++) {
	if (stat_group[tt,tst,ts]) {
	    g = add_stat_group_member(t,st,
				      pmap[stat_probe[tt,tst,ts]],
				      stat_table_size[t,st]);
	} else {
	    g = 0;
	}
	add_stat(t,st,
		 pmap[stat_probe[tt,tst,ts]],
		 g,
		 name "_" stat_name[tt,tst,ts],
		 suffix "\".\"" stat_suffix[tt,tst,ts],
		 stat_type[tt,tst,ts],
		 stat_subtype[tt,tst,ts],
		 stat_agg[tt,tst,ts],
		 stat_width[tt,tst,ts]);
    }
}

#
# data structure update functions
#

# stat_index add_stat(table index, stat table index, stat probe index, stat group index,
#                     stat name, stat suffix, stat type, stat subtype, stat aggregator, stat width)
function add_stat(t,st,p,g,name,suffix,type,subtype,agg,width,  s) {
    dpf("add_stat(" t "," st "," p "," g "," name "," suffix "," type "," subtype "," agg "," width ")");
    s = stat_table_size[t,st]++;
    stat_name[t,st,s] = name;
    stat_suffix[t,st,s] = suffix;
    stat_type[t,st,s] = type;
    stat_subtype[t,st,s] = subtype;
    stat_agg[t,st,s] = agg;
    stat_width[t,st,s] = width;
    stat_probe[t,st,s] = p;
    stat_group[t,st,s] = g;
    return s;
}

# stat_probe_index add_stat_probe(table index, stat table index, probe name, probe suffix, probe mode)
function add_stat_probe(t,st,name,suffix,mode,  p) {
    p = stat_table_probe_count[t,st]++;
    stat_probe_name[t,st,p] = name;
    stat_probe_suffix[t,st,p] = suffix;
    stat_probe_default_mode[t,st,p] = mode;
    stat_group_size[t,st,p] = 0;
    return p;
}

# group_member_index add_stat_group_member(table index, stat table index, stat probe index,
#                                          stat index)
function add_stat_group_member(t,st,p,s,  gm) {
    gm = ++stat_group_size[t,st,p];
    stat_group_member[t,st,p,gm] = s;
    return gm;
}

# void add_action_event(table index, action table index, action index, 
#                       event name, event suffix, default mode)
function add_action_event(t,at,a,en,es,em) {
    action_event_name[t,at,a,action_event_count[t,at,a]] = en;
    action_event_mode[t,at,a,action_event_count[t,at,a]] = em;
    action_event_suffix[t,at,a,action_event_count[t,at,a]++] = es;
}


#
# generator output support
#

# void copyright(void)
function copyright() {
    out("/*");
    out(" * Copyright 2006-2007, Sine Nomine Associates and others.");
    out(" * All Rights Reserved.");
    out(" * ");
    out(" * This software has been released under the terms of the IBM Public");
    out(" * License.  For details, see the LICENSE file in the top-level source");
    out(" * directory or online at http://www.openafs.org/dl/license10.html");
    out(" */");
    out("");
    out("/* !!! WARNING this file is auto-generated by src/trace/table-gen.awk !!! */");
    out("");
}

# void c_top(void)
function c_top() {
    out("#include <osi/osi.h>");
    out("#include <osi/osi_trace.h>");
    out("#include <osi/osi_mem.h>");
    out("#include <osi/osi_list.h>");
    out("#include <trace/generator/directory.h>");
    out("#include <trace/generator/module.h>");
    out("#include <trace/generator/module_reg_queue.h>");
    out("");
    out("");
}

# void c_gen(table index)
function c_gen(t) {
    if (module_h_paths[t] != "") {
	h = module_h_paths[t];
    } else {
	h = "tracepoint_table.h";
    }

    out("#include <" module_dirs[t] "/" h ">");
    out("");
    out("osi_Trace_Module_TracePoint_Table_Decl(" module_names[t] ");");
    out("");

    gen_stats_decls(t);

    out("");
    out("osi_trace_module_init_queue_t " module_init_funcs[t] "_queue_node;");

# emit the initialization function
    out("osi_result");
    out(module_init_funcs[t] "(void)");
    out("{");
    out("    osi_result res;");
    out("    osi_trace_probe_t probe;");
    out("");

# check to make sure libosi is fully online
# if it isn't, register for deferred module initialization
#
# XXX this is sort of a hack; we should have a specific osi_state variable that
# tells us whether it's safe to register probes and modules, rather than relying
# on the main osi state enumeration
    out("    if ((osi_state_current() != OSI_STATE_ONLINE) &&");
    out("        (osi_state_current() != OSI_STATE_INITIALIZING)) {");
    out("        return osi_trace_module_reg_queue_enqueue(&" module_init_funcs[t] "_queue_node,");
    out("                                                  &" module_init_funcs[t] ");");
    out("}");
    out("");

# initialization the module metadata structure
    out("    osi_mem_zero(&osi_Trace_Module_Table_Name(" module_names[t] \
	"), sizeof(osi_Trace_Module_Table_Name(" module_names[t] ")));");

# fill in the header
    out("    osi_Trace_Module_Table_Name(" module_names[t] ").header.version =");
    if (module_versions[t] != "") {
	out("        " module_versions[t] ";");
    } else {
	out("        0;");
    }
    out("    osi_Trace_Module_Table_Name(" module_names[t] ").header.name =");
    out("        \"" module_names[t] "\";");
    out("    osi_Trace_Module_Table_Name(" module_names[t] ").header.prefix =");
    out("        " module_prefixes[t] ";");

    table_probe_count[t] = event_probe_count(t) + action_probe_count(t) + stats_probe_count(t);

    out("    osi_Trace_Module_Table_Name(" module_names[t] ").header.probe_count = ");
    out("        " table_probe_count[t] ";");

    out("    osi_trace_module_register(&osi_Trace_Module_Table_Name(" module_names[t] ").header);");

    gen_event_probe_registration(t);
    gen_action_probe_registration(t);
    gen_stats_probe_registration(t);

    out("    return OSI_OK;");
    out("}");
    out("");
}

# void h_gen(table index)
function h_gen(t,  st) {
    out("#ifndef _AFS_" module_names[t] "_TRACEPOINT_TABLE_H");
    out("#define _AFS_" module_names[t] "_TRACEPOINT_TABLE_H");
    out("");
    out("#include <osi/osi_trace.h>");
    out("#include <osi/osi_stats.h>");
    out("");
    out("#define osi_Trace_" module_names[t] "_ProbeId(probe) osi_Trace_Module_ProbeId(" \
	module_names[t] ", probe)");
    out("");

# emit the probe id table structure
    out("osi_Trace_Module_TracePoint_Table_Def_Begin(" module_names[t] ") {");
    out("    osi_trace_module_header_t header;");
    gen_event_probe_defs(t);
    gen_action_probe_defs(t);
    gen_stats_probe_defs(t);
    out("} osi_Trace_Module_TracePoint_Table_Def_End(" module_names[t] ");");

    out("");
    out("osi_extern osi_result " module_init_funcs[t] "(void);");
    out("");

# emit the stats table definitions
    for (st = 0; st < stat_tables[t]; st++) {
	gen_stats_defs(t, st);
    }

    out("#endif /* _AFS_" module_names[t] "_TRACEPOINT_TABLE_H */");
}

# int event_probe_count(table index)
function event_probe_count(t,  et,e,count) {
    count = 0;
    for (et = 0; et < event_tables[t]; et++) {
	for (e = 0; e < event_table_size[t,et]; e++) {
	    count++;
	}
    }
    return count;
}

# int action_probe_count(table index)
function action_probe_count(t,  at,a,e,count) {
    count = 0;
    for (at = 0; at < action_tables[t]; at++) {
	for (a = 0; a < action_table_size[t,at]; a++) {
	    for (e = 0; e < action_event_count[t,at,a]; e++) {
		count++;
	    }
	}
    }
    return count;
}

# int stats_probe_count(table index)
function stats_probe_count(t,  st,p,count) {
    count = 0;
    for (st = 0; st < stat_tables[t]; st++) {
	if (stat_table_type[t,st] != "template") {
	    for (p = 0; p < stat_table_probe_count[t,st]; p++) {
		count++;
	    }
	}
    }
    return count;
}

# void gen_event_probe_registration(table index)
function gen_event_probe_registration(t,  et,e) {
    for (et = 0; et < event_tables[t]; et++) {
	for (e = 0; e < event_table_size[t,et]; e++) {
	    out("    res = osi_trace_probe_register(&probe, " gen_event_name(t,et,e) ", &osi_Trace_" \
		module_names[t] "_ProbeId(" gen_event_sym(t,et,e) "), " \
		gen_event_mode(t,et,e) ");");
	    out("    if (OSI_RESULT_OK_LIKELY(res)) {");
	    out("        osi_trace_directory_probe_register(" gen_event_name(t,et,e) ", probe);");
	    out("    }");
	    table_probe_count[t]++;
	}
    }
}

# void gen_action_probe_registration(table index)
function gen_action_probe_registration(t,  at,a,e) {
    for (at = 0; at < action_tables[t]; at++) {
	for (a = 0; a < action_table_size[t,at]; a++) {
	    for (e = 0; e < action_event_count[t,at,a]; e++) {
		out("    res = osi_trace_probe_register(&probe, " gen_action_name(t,at,a,e) ", &osi_Trace_" \
		    module_names[t] "_ProbeId(" gen_action_sym(t,at,a,e) "), " \
		    gen_action_mode(t,at,a,e) ");");
		out("    if (OSI_RESULT_OK_LIKELY(res)) {");
		out("        osi_trace_directory_probe_register(" gen_action_name(t,at,a,e) ", probe);");
		out("    }");
		table_probe_count[t]++;
	    }
	}
    }
}

# void gen_stats_probe_registration(table index)
function gen_stats_probe_registration(t,  st,p) {
    for (st = 0; st < stat_tables[t]; st++) {
	if (stat_table_type[t,st] != "template") {
	    for (p = 0; p < stat_table_probe_count[t,st]; p++) {
		out("    res = osi_trace_probe_register(&probe, " gen_stat_probe_name(t,st,p) ", &osi_Trace_" \
		    module_names[t] "_ProbeId(" gen_stat_probe_sym(t,st,p) "), " \
		    gen_stat_probe_mode(t,st,p) ");");
		out("    if (OSI_RESULT_OK_LIKELY(res)) {");
		out("        osi_trace_directory_probe_register(" gen_stat_probe_name(t,st,p) ", probe);");
		out("    }");
		table_probe_count[t]++;
	    }
	}
    }
}

# void gen_event_probe_defs(table index)
function gen_event_probe_defs(t,  et,e) {
    for (et = 0; et < event_tables[t]; et++) {
	for (e = 0; e < event_table_size[t,et]; e++) {
	    out("    OSI_TRACE_PROBE_DECL(" gen_event_sym(t,et,e) ");");
	}
    }
}

# void gen_action_probe_defs(table index)
function gen_action_probe_defs(t,  at,a,e) {
    for (at = 0; at < action_tables[t]; at++) {
	for (a = 0; a < action_table_size[t,at]; a++) {
	    for(e = 0; e < action_event_count[t,at,a]; e++) {
		out("    OSI_TRACE_PROBE_DECL(" gen_action_sym(t,at,a,e) ");");
	    }
	}
    }
}

# void gen_stats_probe_defs(table index)
function gen_stats_probe_defs(t,  st,p) {
    for (st = 0; st < stat_tables[t]; st++) {
	if (stat_table_type[t,st] != "template") {
	    dpf("t[" t "] st[" st "]: stat_table_size = " stat_table_size[t,st]);
	    dpf("t[" t "] st[" st "]: stat_table_probe_count = " stat_table_probe_count[t,st]);
	    for (p = 0; p < stat_table_probe_count[t,st]; p++) {
		out("    OSI_TRACE_PROBE_DECL(" gen_stat_probe_sym(t,st,p) ");");
	    }
	}
    }
}

function gen_event_sym(t,et,e) {
    return event_table_name[t,et] "_" event_name[t,et,e];
}

# string gen_event_name(table index, event table index, event index)
function gen_event_name(t,et,e,  name) {
    if (module_prefixes[t] != "") {
	name = module_prefixes[t] "\".\"";
    } else {
	name = "";
    }
    if (event_table_prefix[t,et] != "") {
	name = name event_table_prefix[t,et] "\".\"";
    }
    return name event_suffix[t,et,e];
}

# int gen_event_mode(table index, event table index, event index)
function gen_event_mode(t,et,e) {
    if (event_default_mode[t,et,e] == "enabled") {
	return 1;
    } else {
	return 0;
    }
}

function gen_action_sym(t,at,a,e) {
    return action_table_name[t,at] "_" action_name[t,at,a] "_" action_event_name[t,at,a,e];
}

# string gen_action_name(table index, action table index, action index, event index)
function gen_action_name(t,at,a,e,  name) {
    if (module_prefixes[t] != "") {
	name = module_prefixes[t] "\".\"";
    } else {
	name = "";
    }
    if (action_table_prefix[t,at] != "") {
	name = name action_table_prefix[t,at] "\".\"";
    }
    return name action_suffix[t,at,a] "\".\"" action_event_suffix[t,at,a,e];
}

# int gen_action_mode(table index, action table index, action index, event index)
function gen_action_mode(t,at,a,e) {
    if (action_event_mode[t,at,a,e] == "enabled") {
	return 1;
    } else {
	return 0;
    }
}

function gen_stat_sym(t,st,s) {
    return stat_table_name[t,st] "_" stat_name[t,st,s];
}

# string gen_stat_name(table index, stat table index, stat index)
function gen_stat_name(t,st,s,  name) {
    if (module_prefixes[t] != "") {
	name = module_prefixes[t] "\".\"";
    } else {
	name = "";
    }
    if (stat_table_prefix[t,st] != "") {
	name = name stat_table_prefix[t,st] "\".\"";
    }
    return name stat_suffix[t,st,s];
}

function gen_stat_probe_sym(t,st,p) {
    return stat_table_name[t,st] "_" stat_probe_name[t,st,p];
}

# string gen_stat_probe_name(table index, stat table index, probe index)
function gen_stat_probe_name(t,st,p,  name) {
    if (module_prefixes[t] != "") {
	name = module_prefixes[t] "\".\"";
    } else {
	name = "";
    }
    if (stat_table_prefix[t,st] != "") {
	name = name stat_table_prefix[t,st] "\".\"";
    }
    return name stat_probe_suffix[t,st,p];
}

# int gen_stat_probe_mode(table index, stat table index, probe index)
function gen_stat_probe_mode(t,st,p) {
    if (stat_probe_default_mode[t,st,p] == "enabled") {
	return 1;
    } else {
	return 0;
    }
}

#
# generate declarations for
# global stats tables
#

# void gen_stats_decls(table index)
function gen_stats_decls(t,st) {
    for (st = 0; st < stat_tables[t]; st++) {
	if ((stat_table_type[t,st] == "global") ||
	    ((stat_table_type[t,st] != "template") &&
	     (stat_table_global_agg[t,st] == "yes"))) {
	    out(gen_stat_table_type_name(t,st) " " gen_stat_table_global_name(t,st) ";");
	}
    }
}

#
# generate the definitions for a
# stats table
#

# void gen_stats_defs(table index, stats table index)
function gen_stats_defs(t,st) {
    if (stat_table_type[t,st] != "template") {
	out("/*");
	out(" * " stat_table_name[t,st]);
	out(" * statistics collection");
	out(" */");
	gen_stats_struct(t,st);
	if (stat_table_type[t,st] == "context-local") {
	    gen_stats_local_struct(t,st);
	}
	gen_stats_group_enums(t,st);
	gen_stats_macros(t,st);
	out("");
	out("");
    }
}


#
# generate definition for a 
# global or object-local stats table
# NOTE: context-local stats tables still build
#       this definition for the global aggregator
#       table
#

function gen_stat_table_struct_name(t,st) {
    return module_names[t] "_" stat_table_name[t,st];
}

function gen_stat_table_type_name(t,st) {
    return gen_stat_table_struct_name(t,st) "_t";
}

function gen_stat_table_global_name(t,st) {
    return gen_stat_table_struct_name(t,st);
}



function gen_stat_type_name(t,st,s) {
    if (stat_type[t,st,s] == "integer") {
	return "osi_stats_" stat_agg[t,st,s] stat_width[t,st,s] "_t";
    } else if (stat_type[t,st,s] == "time") {
	return "osi_stats_" stat_type[t,st,s] stat_width[t,st,s] "_t";
    }
}

# void gen_stats_struct(table index, stats table index)
function gen_stats_struct(t,st,  s) {
    out("typedef struct " gen_stat_table_struct_name(t,st) " {");
    for (s = 0; s < stat_table_size[t,st]; s++) {
	out("    " gen_stat_type_name(t,st,s) " " stat_name[t,st,s] ";");
    }
    if ((stat_table_sync[t,st] == "explicit") ||
	(stat_table_sync[t,st] == "implicit") ||
	(stat_table_type[t,st] == "context-local")) {
	out("    osi_mutex_t lock;");
    }
    out("} " gen_stat_table_type_name(t,st) ";");
    if ((stat_table_type[t,st] == "global") ||
	(stat_table_type[t,st] == "context-local")) {
	out("osi_extern " gen_stat_table_type_name(t,st) " " gen_stat_table_global_name(t,st) ";");
    }
    out("");
}


#
# generate a definition for a 
# context-local stats table
#

function gen_stat_table_local_struct_name(t,st) {
    return gen_stat_table_struct_name(t,st) "_local_ctx";
}

function gen_stat_table_local_type_name(t,st) {
    return gen_stat_table_local_struct_name(t,st) "_t";
}

function gen_stat_local_type_name(t,st,s) {
    return "osi_stats_" stat_agg[t,st,s] stat_width[t,st,s] "_local_ctx_t";
}

function gen_stats_local_key_name(t,st) {
    return gen_stat_table_local_struct_name(t,st) "_key";
}

function gen_stats_local_key_var(t,st) {
    return gen_stats_local_key_name(t,st);
}

function gen_stats_local_key_ptr(t,st) {
    return "&" gen_stats_local_key_var(t,st);
}

function gen_stats_local_initfunc_name(t,st) {
    return gen_stat_table_local_struct_name(t,st) "_init";
}

# void gen_stats_local_struct(table index, stats table index)
function gen_stats_local_struct(t,st,  s) {
    out("#if defined(OSI_STATS_CTX_LOCAL_BUFFER)");
    out("typedef struct " gen_stat_table_local_struct_name(t,st) " {");
    if (stat_table_storage_class[t,st] == "dynamic") {
	out("    osi_list_element_volatile ctx_list;");
    }
    for (s = 0; s < stat_table_size[t,st]; s++) {
	out("    " gen_stat_local_type_name(t,st,s) " " stat_name[t,st,s] ";");
    }
    out("} " gen_stat_table_local_type_name(t,st) ";");
    out("osi_extern osi_mem_local_key_t " gen_stats_local_key_name(t,st) ";");
    out("osi_extern " gen_stat_table_local_type_name(t,st) " * " gen_stats_local_initfunc_name(t,st) "(void);");
    out("#endif /* OSI_STATS_CTX_LOCAL_BUFFER */");
    out("");
}


#
# generate stats group enums
#

# void gen_stats_group_enums(table index, stats table index)
function gen_stats_group_enums(t,st,  p,s) {
    for (p = 0; p < stat_table_probe_count[t,st]; p++) {
	if (stat_group_size[t,st,p]) {
	    s = stat_group_member[t,st,p,1];
	    if (stat_group[t,st,s] != 0) {
		gen_stats_group_enum(t,st,p);
	    }
	}
    }
}

function gen_stats_group_enum_name(t,st,p) {
    return gen_stat_probe_sym(t,st,p);
}

function gen_stats_group_enum_type_name(t,st,p) {
    return gen_stats_group_enum_name(t,st,p) "_t";
}

function gen_stat_enum_sym(t,st,p,s) {
    return gen_stats_group_enum_name(t,st,p) "_" stat_name[t,st,s] "_stat_id";
}

function gen_stats_group_enum(t,st,p,  g,s) {
    out("typedef enum " gen_stats_group_enum_name(t,st,p) " {");
    for (g = 1; g <= stat_group_size[t,st,p]; g++) {
	s = stat_group_member[t,st,p,g];
	out("    " gen_stat_enum_sym(t,st,p,s) ",");
    }
    out("    " gen_stats_group_enum_name(t,st,p) "_all_id");
    out("} " gen_stats_group_enum_type_name(t,st,p) ";");
}

#
# generate stats update macros
#

# void gen_stats_macros(table index, stats table index)
function gen_stats_macros(t,st,  s) {
    for (s = 0; s < stat_table_size[t,st]; s++) {
	if (stat_table_type[t,st] == "global") {
	    gen_stats_global_macros(t,st,s);
	} else if (stat_table_type[t,st] == "object-local") {
	    gen_stats_object_macros(t,st,s);
	} else if (stat_table_type[t,st] == "context-local") {
	    gen_stats_ctx_macros(t,st,s);
	} else if (stat_table_type[t,st] == "template") {
	    
	} else {
	    panic("bad stats table type");
	}
    }
}

function gen_stat_macro_name(t,st,s,op) {
    return "osi_stats_" module_names[t] "_" stat_table_name[t,st] "_" stat_name[t,st,s] "_" op;
}

function gen_stat_probe_id_ref(t,st,s) {
    return "osi_Trace_" module_names[t] "_ProbeId(" gen_stat_probe_sym(t,st,stat_probe[t,st,s]) ")";
}

function gen_osi_stat_call_func(t,st,s,op) {
    return "osi_stats_" stat_agg[t,st,s] stat_width[t,st,s] "_" op;
}

function gen_osi_stat_local_call_func(t,st,s,op) {
    return "osi_stats_local_" stat_agg[t,st,s] stat_width[t,st,s] "_" op;
}


#
# generate the macro interfaces used to update
# a statistic stored in a global stats table.
#
function gen_stats_global_macros(t,st,s) {
    if ((stat_table_sync[t,st] == "external") ||
	(stat_table_sync[t,st] == "explicit")) {
	if (stat_type[t,st,s] == "integer") {
	    if ((stat_agg[t,st,s] == "level") ||
		(stat_agg[t,st,s] == "counter")) {
		out(gen_osi_stat_global_macro(t,st,s,"inc","inc",""));
		out(gen_osi_stat_global_macro(t,st,s,"add","add","val"));
		if (stat_width[t,st,s] == 64) {
		    out(gen_osi_stat_global_macro(t,st,s,"add32","add32","val"));
		}
	    }
	    if (stat_agg[t,st,s] == "level") {
		out(gen_osi_stat_global_macro(t,st,s,"dec","dec",""));
		out(gen_osi_stat_global_macro(t,st,s,"sub","sub","val"));
		if (stat_width[t,st,s] == 64) {
		    out(gen_osi_stat_global_macro(t,st,s,"sub32","sub32","val"));
		}
	    }
	}
    } else if (stat_table_sync[t,st] == "implicit") {
	if (stat_type[t,st,s] == "integer") {
	    if ((stat_agg[t,st,s] == "level") ||
		(stat_agg[t,st,s] == "counter")) {
		out(gen_osi_stat_global_macro(t,st,s,"inc","inc_atomic",""));
		out(gen_osi_stat_global_macro(t,st,s,"add","add_atomic","val"));
		if (stat_width[t,st,s] == 64) {
		    out(gen_osi_stat_global_macro(t,st,s,"add32","add32_atomic","val"));
		}
	    }
	    if (stat_agg[t,st,s] == "level") {
		out(gen_osi_stat_global_macro(t,st,s,"dec","dec_atomic",""));
		out(gen_osi_stat_global_macro(t,st,s,"sub","sub_atomic","val"));
		if (stat_width[t,st,s] == 64) {
		    out(gen_osi_stat_global_macro(t,st,s,"sub32","sub32_atomic","val"));
		}
	    }
	}
    }
    out("");
}

# string gen_osi_stat_global_call(table index, stat table index, stat index, operator, variable arg list)
function gen_osi_stat_global_call(t,st,s,op,var,  arg,grp_mbr) {
    arg = (var == "") ? "" : ", " var;
    if (stat_group[t,st,s]) {
	grp_mbr = gen_stat_enum_sym(t,st,stat_probe[t,st,s],s);
    } else {
	grp_mbr = 0;
    }
    return gen_osi_stat_call_func(t,st,s,op) "(" gen_stat_probe_id_ref(t,st,s) ", " grp_mbr \
	", " module_events[t] ", " gen_osi_stat_global_stat_ptr(t,st) ", " stat_name[t,st,s] arg ")";
}

# string gen_osi_stat_global_macro(table index, stat table index, stat index, stat name, operator, var args)
function gen_osi_stat_global_macro(t,st,s,name,op,var) {
    return "#define " gen_stat_macro_name(t,st,s,name) "(" var ") \\\n    " \
	gen_osi_stat_global_call(t,st,s,op,var);
}

# string gen_osi_stat_global_stat_ptr(table index, stat table index)
function gen_osi_stat_global_stat_ptr(t,st) {
    return "&" gen_stat_table_global_name(t,st);
}

#
# generate the macro interfaces used to update
# a statistic stored in an object-local stats table.
#

# void gen_stats_object_macros(table index, stat table index, stat index)
function gen_stats_object_macros(t,st,s) {
    if ((stat_table_sync[t,st] == "external") ||
	(stat_table_sync[t,st] == "explicit")) {
	if (stat_type[t,st,s] == "integer") {
	    if ((stat_agg[t,st,s] == "level") ||
		(stat_agg[t,st,s] == "counter")) {
		out(gen_osi_stat_object_macro(t,st,s,"inc","inc",""));
		out(gen_osi_stat_object_macro(t,st,s,"add","add","val"));
		if (stat_width[t,st,s] == 64) {
		    out(gen_osi_stat_object_macro(t,st,s,"add32","add32","val"));
		}
	    }
	    if (stat_agg[t,st,s] == "level") {
		out(gen_osi_stat_object_macro(t,st,s,"dec","dec",""));
		out(gen_osi_stat_object_macro(t,st,s,"sub","sub","val"));
		if (stat_width[t,st,s] == 64) {
		    out(gen_osi_stat_object_macro(t,st,s,"sub32","sub32","val"));
		}
	    }
	}
    } else if (stat_table_sync[t,st] == "implicit") {
	if (stat_type[t,st,s] == "integer") {
	    if ((stat_agg[t,st,s] == "level") ||
		(stat_agg[t,st,s] == "counter")) {
		out(gen_osi_stat_object_macro(t,st,s,"inc","inc_atomic",""));
		out(gen_osi_stat_object_macro(t,st,s,"add","add_atomic","val"));
		if (stat_width[t,st,s] == 64) {
		    out(gen_osi_stat_object_macro(t,st,s,"add32","add32_atomic","val"));
		}
	    }
	    if (stat_agg[t,st,s] == "level") {
		out(gen_osi_stat_object_macro(t,st,s,"dec","dec_atomic",""));
		out(gen_osi_stat_object_macro(t,st,s,"sub","sub_atomic","val"));
		if (stat_width[t,st,s] == 64) {
		    out(gen_osi_stat_object_macro(t,st,s,"sub32","sub32_atomic","val"));
		}
	    }
	}
    }
    out("");
}

function gen_osi_stat_object_member_ptr(t,st,this) {
    return "&((" this ")->" stat_table_object_member[t,st] ")";
}

# string gen_osi_stat_object_call(table index, stat table index, stat index, operator, 
#                                 stat struct member name, variable argument list)
function gen_osi_stat_object_call(t,st,s,op,member,var,  arg,grp_mbr) {
    arg = (var == "") ? "" : ", " var;
    if (stat_group[t,st,s]) {
	grp_mbr = gen_stat_enum_sym(t,st,stat_probe[t,st,s],s);
    } else {
	grp_mbr = 0;
    }
    return gen_osi_stat_call_func(t,st,s,op) "(" gen_stat_probe_id_ref(t,st,s) \
	", " grp_mbr ", " module_events[t] ", " member ", " stat_name[t,st,s] arg ")";
}

# string gen_osi_stat_object_macro(table index, stat table index, stat index, stat name, operator,
#                                  variable argument list)
function gen_osi_stat_object_macro(t,st,s,name,op,var,  arg,this) {
    arg = (var == "") ? "" : ", " var;
    this = "this";
    return "#define " gen_stat_macro_name(t,st,s,name) "(" this arg ") \\\n    " \
	gen_osi_stat_object_call(t,st,s,op,gen_osi_stat_object_member_ptr(t,st,this),var);
}

#
# generate the macro interfaces used to update
# a statistic stored in a context-local stats table.
#

# void gen_stats_ctx_macros(table index, stat table index, stat index)
function gen_stats_ctx_macros(t,st,s) {
    out("#if defined(OSI_STATS_CTX_LOCAL_BUFFER)");
    if (stat_type[t,st,s] == "integer") {
	if ((stat_agg[t,st,s] == "level") ||
	    (stat_agg[t,st,s] == "counter")) {
	    out(gen_osi_stat_ctx_macro(t,st,s,"inc","inc",""));
	    out(gen_osi_stat_ctx_macro(t,st,s,"add","add","val"));
	    if (stat_width[t,st,s] == 64) {
		out(gen_osi_stat_ctx_macro(t,st,s,"add32","add32","val"));
	    }
	}
	if (stat_agg[t,st,s] == "level") {
	    out(gen_osi_stat_ctx_macro(t,st,s,"dec","dec",""));
	    out(gen_osi_stat_ctx_macro(t,st,s,"sub","sub","val"));
	    if (stat_width[t,st,s] == 64) {
		out(gen_osi_stat_ctx_macro(t,st,s,"sub32","sub32","val"));
	    }
	}
    }
    out("#else /* !OSI_STATS_CTX_LOCAL_BUFFER */");
    if (stat_type[t,st,s] == "integer") {
	if ((stat_agg[t,st,s] == "level") ||
	    (stat_agg[t,st,s] == "counter")) {
	    out(gen_osi_stat_global_macro(t,st,s,"inc","inc_atomic",""));
	    out(gen_osi_stat_global_macro(t,st,s,"add","add_atomic","val"));
	    if (stat_width[t,st,s] == 64) {
		out(gen_osi_stat_global_macro(t,st,s,"add32","add32_atomic","val"));
	    }
	}
	if (stat_agg[t,st,s] == "level") {
	    out(gen_osi_stat_global_macro(t,st,s,"dec","dec_atomic",""));
	    out(gen_osi_stat_global_macro(t,st,s,"sub","sub_atomic","val"));
	    if (stat_width[t,st,s] == 64) {
		out(gen_osi_stat_global_macro(t,st,s,"sub32","sub32_atomic","val"));
	    }
	}
    }
    out("#endif /* !OSI_STATS_CTX_LOCAL_BUFFER */");
    out("");
}

# string gen_osi_stat_ctx_call(table index, stat table index, stat index, operator,
#                              this pointer arg name, variable argument list)
function gen_osi_stat_ctx_call(t,st,s,op,this,var,  arg,grp_mbr) {
    arg = (var == "") ? "" : ", " var;
    if (stat_group[t,st,s]) {
	grp_mbr = gen_stat_enum_sym(t,st,stat_probe[t,st,s],s);
    } else {
	grp_mbr = 0;
    }
    return gen_osi_stat_local_call_func(t,st,s,op) "(" gen_stat_probe_id_ref(t,st,s) \
	", " grp_mbr ", " module_events[t] ", " this ", " gen_osi_stat_global_stat_ptr(t,st) \
	", " stat_name[t,st,s] arg ")";
}

# string gen_osi_stat_ctx_macro(table index, stat table index, stat index, stat name, operator
#                               variable argument list)
function gen_osi_stat_ctx_macro(t,st,s,name,op,var,  this,class,call) {
    this = "__osi_st_lctx";
    class = stat_table_storage_class[t,st];
    call = "#define " gen_stat_macro_name(t,st,s,name) "(" var ") \\\n" \
	"    _osi_stats_ctx_local_op_" class "(" \
	gen_stats_local_key_var(t,st) ", \\\n" \
	"                                   " gen_stat_table_local_type_name(t,st) ", \\\n" \
	"                                   " this ", \\\n" \
	"                                   " \
	gen_osi_stat_ctx_call(t,st,s,op, this, var);

    if (class == "dynamic") {
	call = call ", \\\n" \
	    "                                   " \
	    gen_stats_local_initfunc_name(t,st) "()" ;
    }
    call = call ")";
    return call;
}
