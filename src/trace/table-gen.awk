#
# Copyright 2006, Sine Nomine Associates and others.
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

# allocate a unique id for each lexical scope
    scope_global = tidx++;
    scope_table_wait_brace = tidx++;
    scope_table = tidx++;
    scope_event_table_wait_brace = tidx++;
    scope_event_table = tidx++;
    scope_action_table_wait_brace = tidx++;
    scope_action_table = tidx++;
    scope_stat_table_wait_brace = tidx++;
    scope_stat_table = tidx++;
    scope_query_table_wait_brace = tidx++;
    scope_query_table = tidx++;
    scope_event_wait_brace = tidx++;
    scope_event = tidx++;
    scope_action_wait_brace = tidx++;
    scope_action = tidx++;
    scope_action_event_wait_brace = tidx++;
    scope_action_event = tidx++;
    scope_stat_group_wait_brace = tidx++;
    scope_stat_group = tidx++;
    scope_stat_wait_brace = tidx++;
    scope_stat = tidx++;
    scope_vstat_wait_brace = tidx++;
    scope_vstat = tidx++;
    scope_stat_table_inherit_wait_brace = tidx++;
    scope_stat_table_inherit = tidx++;
    scope_query_wait_brace = tidx++;
    scope_query = tidx++;
    scope_description = tidx++;


    scope_init(scope_global);

    dpf("OpenAFS Instrumentation Framework");
    dpf("Automated Code Generator v0.1");
    dpf("Copyright (c) 2006, Sine Nomine Associates and others.");
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

    } else if (scope == scope_description) {
	if (brace_depth < desc_brace_depth) {
	    scope_pop();
	}
    } else if ($1 == "description") {
	if ($2 == "{") {
	    scope_push(scope_description);
	    desc_brace_depth = brace_depth;
	}
    } else if (scope == scope_global) {
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
    } else if (scope == scope_table_wait_brace) {
	if ($1 == "{") {
	    scope_current_replace(scope_table);
	} else {
	    parse_panic("expected token '{' missing after token 'trace-table'");
	}
    } else if (scope == scope_table) {
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
	} else if ($1 == "version") {
	    module_version[table] = $2;
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
    } else if (scope == scope_event_table_wait_brace) {
	if ($1 == "{") {
	    scope_current_replace(scope_event_table);
	} else {
	    parse_panic("expected token '{' missing after token 'event-table'");
	}
    } else if (scope == scope_event_table) {
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
    } else if (scope == scope_event_wait_brace) {
	if ($1 == "{") {
	    scope_current_replace(scope_event);
	} else {
	    parse_panic("expected token '{' missing after token 'event'");
	}
    } else if (scope == scope_event) {
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
    } else if (scope == scope_action_table_wait_brace) {
	if ($1 == "{") {
	    scope_current_replace(scope_action_table);
	} else {
	    parse_panic("expected token '{' missing after token 'action-table'");
	}
    } else if (scope == scope_action_table) {
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
    } else if (scope == scope_action_wait_brace) {
	if ($1 == "{") {
	    scope_current_replace(scope_action);
	} else {
	    parse_panic("expected token '{' missing after token 'action'");
	}
    } else if (scope == scope_action) {
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
    } else if (scope == scope_action_event_wait_brace) {
	if ($1 == "{") {
	    scope_current_replace(scope_action_event);
	} else {
	    parse_panic("expected token '{' missing after token 'event'");
	}
    } else if (scope == scope_action_event) {
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
    } else if (scope == scope_stat_table_wait_brace) {
	if ($1 == "{") {
	    scope_current_replace(scope_stat_table);
	} else {
	    parse_panic("expected token '{' missing after token 'stats-table'");
	}
    } else if (scope == scope_stat_table) {
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
    } else if (scope == scope_stat_wait_brace) {
	if ($1 == "{") {
	    scope_current_replace(scope_stat);
	} else {
	    parse_panic("expected token '{' missing after token 'stat'");
	}
    } else if (scope == scope_stat) {
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
    } else if (scope == scope_vstat_wait_brace) {
	if ($1 == "{") {
	    scope_current_replace(scope_vstat);
	} else {
	    parse_panic("expected token '{' missing after token 'vstat'");
	}
    } else if (scope == scope_vstat) {
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
    } else if (scope == scope_stat_group_wait_brace) {
	if ($1 == "{") {
	    scope_current_replace(scope_stat_group);
	} else {
	    parse_panic("expected token '{' missing after token 'group'");
	}
    } else if (scope == scope_stat_group) {
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
    } else if (scope == scope_stat_table_inherit_wait_brace) {
	if ($1 == "{") {
	    scope_current_replace(scope_stat_table_inherit);
	} else {
	    parse_panic("expected token '{' missing after token 'inherit'");
	}
    } else if (scope == scope_stat_table_inherit) {
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
    } else if (scope == scope_query_table_wait_brace) {
	if ($1 == "{") {
	    scope_current_replace(scope_query_table);
	} else {
	    parse_panic("expected token '{' missing after token 'query-table'");
	}
    } else if (scope == scope_query_table) {
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
    if (scope != scope_global) {
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
    dpf("*** syntax error around " FILENAME ":" FNR " at scope " scope " ***");
    dpf("     " s);
    scope_dump();
    panic("parsing cannot continue");
}

#
# parser scoping control
#

# void scope_init(new_scope)
function scope_init(new_scope) {
    scope_stack_depth = 0;
    scope_push(new_scope);
}

# old_scope scope_push(new_scope)
function scope_push(new_scope,  save_scope) {
    save_scope = scope;

    scope_stack[++scope_stack_depth] = new_scope;
    scope = new_scope;

    scope_local_push();

    return save;
}

# restore_scope scope_pop(void)
function scope_pop() {
    scope_local_pop();
    scope = scope_stack[--scope_stack_depth];
    return scope;
}

# parent_scope scope_peek(void)
function scope_peek() {
    return scope_stack[scope_stack_depth-1];
}

# void scope_current_replace(scope)
function scope_current_replace(new_scope) {
    scope = scope_stack[scope_stack_depth] = new_scope;
}

# void scope_dump(void)
function scope_dump(  s) {
    dpf("*** begin scope stack dump ***");
    for (s = 1; s <= scope_stack_depth; s++) {
	dpf("     s[" s "]: " scope_stack[s]);
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
    out(" * Copyright 2006, Sine Nomine Associates and others.");
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
    out("#include <trace/generator/directory.h>");
    out("#include <trace/generator/module.h>");
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

    out("osi_result");
    out(module_init_funcs[t] "(void)");
    out("{");
    out("    osi_result res;");
    out("    osi_trace_probe_t probe;");
    out("");
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
	    out("    res = osi_trace_probe_register(&probe, &osi_Trace_" \
		module_names[t] "_ProbeId(" gen_event_sym(t,et,e) "), " \
		gen_event_mode(t,et,e) ");");
	    out("    if (OSI_RESULT_OK_LIKELY(res)) {");
	    out("        osi_trace_directory_probe_register(" gen_event_name(t,et,e) ", probe);");
	    out("    }");
#	    out("    res = osi_trace_probe_register(" gen_event_name(t,et,e) ", &osi_Trace_" \
#		module_names[t] "_ProbeId(" gen_event_sym(t,et,e) "));");
	    table_probe_count[t]++;
	}
    }
}

# void gen_action_probe_registration(table index)
function gen_action_probe_registration(t,  at,a,e) {
    for (at = 0; at < action_tables[t]; at++) {
	for (a = 0; a < action_table_size[t,at]; a++) {
	    for (e = 0; e < action_event_count[t,at,a]; e++) {
		out("    res = osi_trace_probe_register(&probe, &osi_Trace_" \
		    module_names[t] "_ProbeId(" gen_action_sym(t,at,a,e) "), " \
		    gen_action_mode(t,at,a,e) ");");
		out("    if (OSI_RESULT_OK_LIKELY(res)) {");
		out("        osi_trace_directory_probe_register(" gen_action_name(t,at,a,e) ", probe);");
		out("    }");
#		out("    osi_trace_probe_register(" gen_action_name(t,at,a,e) ", &osi_Trace_" \
#		    module_names[t] "_ProbeId(" gen_action_sym(t,at,a,e) "));");
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
		out("    res = osi_trace_probe_register(&probe, &osi_Trace_" \
		    module_names[t] "_ProbeId(" gen_stat_probe_sym(t,st,p) "), " \
		    gen_stat_probe_mode(t,st,p) ");");
		out("    if (OSI_RESULT_OK_LIKELY(res)) {");
		out("        osi_trace_directory_probe_register(" gen_stat_probe_name(t,st,p) ", probe);");
		out("    }");
#		out("    osi_trace_probe_register(" gen_stat_probe_name(t,st,p) ", &osi_Trace_" \
#		    module_names[t] "_ProbeId(" gen_stat_probe_sym(t,st,p) "));");
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
