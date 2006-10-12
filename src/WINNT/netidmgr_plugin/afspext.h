/*
 * Copyright (c) 2004 Massachusetts Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/* $Id$ */

#ifndef __AFSPLUGIN_EXT_H
#define __AFSPLUGIN_EXT_H

/*! \defgroup afs_ext OpenAFS Plugin extensions 

    This section documents messages and data structures used by AFS
    extension plugins.  These are plugins which augment the behavior
    of the AFS plugin.

    When performing specific tasks for NetIDMgr, the AFS plugin will
    send out messages to the extension plugins either via broadcast or
    unicast.  The extension plugins provide functionality by
    responding to these messages.

  @{*/

#define MAXCELLCHARS   64
#define MAXHOSTCHARS   64
#define MAXHOSTSPERCELL 8

#define TRANSARCAFSDAEMON "TransarcAFSDaemon"

#define AFS_TOKENNAME_AUTO L"Auto"
#define AFS_TOKENNAME_KRB5 L"Kerberos5"
#define AFS_TOKENNAME_KRB524 L"Kerberos524"
#define AFS_TOKENNAME_KRB4 L"Kerberos4"

/*! \brief An AFS token acquisition method identifier 

    This takes on a value from ::afs_token_method or a token
    acquisition method identifier assigned to an extension plugin.
*/
typedef khm_int32 afs_tk_method;

/*! \brief Predefined token acquisition methods */
enum afs_token_method {
    AFS_TOKEN_AUTO = 0,         /*!< Automatic.  This method iterates
                                  through Krb5, Krb524, Krb4 and then
                                  any extensions which provide token
                                  acquisition methods until one of
                                  them succeeds. */
    AFS_TOKEN_KRB5,             /*!< Kerberos 5 */
    AFS_TOKEN_KRB524,           /*!< Kerberos 5 with krb524 translation */
    AFS_TOKEN_KRB4,             /*!< Kerberos 4 */
};

/*! \brief Version of the OpenAFS Plugin

    This is an internal number that identifies the version of the
    OpenAFS plugin API that this extension was built against.  This
    number is specified when sending the ::AFS_MSG_ANNOUNCE message.
 */
#define AFS_PLUGIN_VERSION 0x0000001

/*! \name Messages

    The AFS plugin registers the message type named ::AFS_MSG_TYPENAME
    and sends messages of this type to notify any AFS extension
    plugins to notify them of various events.

 @{*/

/*! \brief Name of the AFS plugin message

    This message type is registered when the AFS plugin starts and is
    unregistered when the plugin stops.

    Use kmq_find_type() to find the type ID of this message type.
 */
#define AFS_MSG_TYPENAME L"AfsExtMessage"

/*! \brief Announce an extension plugin

    Sent by an extension plugin to announce its existence to the AFS
    plugin.  This message should be sent by the extension plugin when
    it has finished loading, and is the only message permitted to be
    sent by an extension. All other messages are sent by the AFS
    plugin.

    Since this message contains pointer parameters and there is no
    cleanup performed on this, the message should be sent using
    kmq_send_message().

    <table>
    <tr><td>Type</td><td>type ID of ::AFS_MSG_TYPENAME</td></tr>
    <tr><td>Subtype</td><td>::AFS_MSG_ANNOUNCE</td></tr>
    <tr><td>uparam</td><td>0</td></tr>
    <tr><td>vparam</td><td>Pointer to a ::afs_msg_announce structure</td></tr>
    </table>

    \note This message is only sent from extension plugins to the AFS plugin.
 */
#define AFS_MSG_ANNOUNCE 1

/*! \brief Parameter structure for announcing an extension plugin

    \see ::AFS_MSG_ANNOUNCE
 */
typedef struct tag_afs_msg_announce_v1 {
    khm_size  cbsize;           /*!< Size of the strucutre.  Set to \a
                                  sizeof(::afs_msg_announce).  If
                                  there is a version skew between the
                                  AFS plugin and the extension, then
                                  this parameter will ensure that the
                                  AFS plugin understands the correct
                                  version of the structure. */

    khm_ui_4  version;          /*!< Version of the AFS plugin that
                                  the extension is compiled for.  Set
                                  this to ::AFS_PLUGIN_VERSION.
                                  Depending on this value, the AFS
                                  plugin will either reject the
                                  extension or determine which set of
                                  messages and structures should be
                                  used to communicate with the
                                  extension. */

    const wchar_t * name;       /*!< Name of the extension.  Should be
                                  unique among all AFS extension
                                  plugins. Size constrained by
                                  ::KHUI_MAXCCH_NAME*/

    khm_handle sub;             /*!< A valid subscription for unicast
                                  messages.  This must have been
                                  created through
                                  kmq_create_subscription().  The
                                  supplied handle will be
                                  automatically released when the
                                  plugin exits.  However, if the
                                  announcement message fails, then the
                                  extension has to release the handle
                                  itself. */

    khm_boolean provide_token_acq; /*!< non-zero if the extension
                                     provides a token acquisition
                                     method. The \a token_acq
                                     substructure should be filled if
                                     this member is set to
                                     non-zero. */

    struct {
        const wchar_t * short_desc; /*!< Short description of token
                                  acquisition method. (localized,
                                  required).  Size is constrained by
                                  ::KHUI_MAXCCH_SHORT_DESC */

        const wchar_t * long_desc; /*!< Long description.  (localized,
                                  optional).  Size is constrained by
                                  ::KHUI_MAXCCH_LONG_DESC */

        afs_tk_method method_id; /*!< Once the message is processed,
                                  this will receive a new method
                                  identifier.  The value of this field
                                  on entry is ignored. */

    } token_acq;                /*!< Registration information for
                                  token acquisition method.  Only
                                  assumed to be valid if \a
                                  provide_token_acq is TRUE. */

} afs_msg_announce;

/*! \brief Sent to all extensions to resolve the identity of a token

    If the identity and credentials acquisition method of an AFS token
    cannot be determined by the AFS plugin, this message is sent out
    to extension plugins to allow them a chance to resolve it.

    If the extension plugin successfully resolves the identity and
    token acquisition method of the specified token, it should return
    ::KHM_ERROR_SUCCESS.  Otherwise it should return a failure code.
    The actual return code is not interpreted other than whether or
    not it passes the ::KHM_SUCCEEDED() test.

    <table>
    <tr><td>Type</td><td>type ID of ::AFS_MSG_TYPENAME</td></tr>
    <tr><td>Subtype</td><td>::AFS_MSG_RESOLVE_TOKEN</td></tr>
    <tr><td>uparam</td><td>0</td></tr>
    <tr><td>vparam</td><td>Pointer to a ::afs_msg_resolve_token structure</td></tr>
    </table>

    \note This message is only sent from the AFS plugin to extension plugins

    \note Only sent if the extension plugin has ::provide_token_acq set.
 */
#define AFS_MSG_RESOLVE_TOKEN 2

/*! \brief Message structure for AFS_MSG_RESOLVE_TOKEN

    Other than the fields marked as \a [OUT], all other fields should
    be considered read-only and should not be modified.

    \see ::AFS_MSG_RESOLVE_TOKEN
 */
typedef struct tag_afs_msg_resolve_token_v1 {
    khm_size cbsize;            /*!< Size of the structure.  This will
                                  be set to \a
                                  sizeof(::afs_msg_resolve_token). */

    const wchar_t * cell;       /*!< Specifies the cell that the token
                                  belongs to. */

    const struct ktc_token * token; /*!< The token */
    const struct ktc_principal * serverp; /*!< Server principal */
    const struct ktc_principal * clientp; /*!< Client principal */

    khm_handle ident;           /*!< [OUT] If the extension
                                  successfully resolves the identity,
                                  then it should assign a handle to
                                  the identity to this field and
                                  return ::KHM_ERROR_SUCCESS.  The
                                  handle will be automatically freed
                                  by the AFS plugin. */

    afs_tk_method method;       /*!< [OUT] If the extension
                                  successfully resolves the identity,
                                  it should also assign the token
                                  acquisition method identifier to
                                  this field.  The default method is
                                  ::AFS_TOKEN_AUTO.  This field
                                  indicates the token acquisition
                                  method that was used to obtain the
                                  token and is used when the token
                                  needs to be renewed. */
} afs_msg_resolve_token;

/*! \brief Sent to an extension plugin to obtain AFS tokens

    <table>
    <tr><td>Type</td><td>type ID of ::AFS_MSG_TYPENAME</td></tr>
    <tr><td>Subtype</td><td>::AFS_MSG_KLOG</td></tr>
    <tr><td>uparam</td><td>0</td></tr>
    <tr><td>vparam</td><td>Pointer to a ::afs_msg_klog</td></tr>
    </table>

    \note Only sent from the AFS plugin to extension plugins
    \note Only sent to extension plugins which have ::provide_token_acq set.
*/
#define AFS_MSG_KLOG 3

/*! \brief Cell configuration information

    \see ::afs_msg_klog

    \note This structure uses ANSI char fields instead of unicode fields.
 */
typedef struct tag_afs_conf_cellA_v1 {
    khm_size cbsize;            /*!< set to \a sizeof(afs_conf_cell) */

    char     name[MAXCELLCHARS]; /*!< Name of the cell */
    short    numServers;        /*!< Number of servers for cell.
                                  Upper bound of MAXHOSTSPERCELL */
    short    flags;             /*!< Not used. Set to zero. */
    struct sockaddr_in hostAddr[MAXHOSTSPERCELL];
                                /*!< addresses for each server.  There
                                  are \a numServers entries.*/
    char     hostName[MAXHOSTSPERCELL][MAXHOSTCHARS];
                                /*!< names of the servers. There are
                                  \a numServers entries. */
    char *   linkedCell;        /*!< Not used.  Set to zero. */
} afs_conf_cell;

/*! \brief Message parameters for AFS_MSG_KLOG message

    \see ::AFS_MSG_KLOG

    \note This structure uses ANSI char fields instead of unicode fields.
 */
typedef struct tag_afs_msg_klogA_v1 {
    khm_size        cbsize;     /*!< Set to \a sizeof(afs_msg_klog) */

    khm_handle      identity;   /*!< Handle to identity for which we
                                  are obtaining tokens. */

    const char *    service;    /*!< Service name to use when
                                  obtaining token.  This can be NULL
                                  if the service name has not be
                                  determined. */

    const char *    cell;       /*!< Name of cell to obtain tokens
                                  for.  Can be NULL if the local cell
                                  is to be used. */

    const char *    realm;      /*!< Realm to use when obtaining
                                  tokens.  Can be NULL if the realm
                                  has not been determined. */

    const afs_conf_cell * cell_config; /*!< Cell configuration for the
                                   cell specified in \a cell. */

    khm_int32       lifetime;   /*!< Advisory lifetime specifier, in
                                  seconds.  If set to zero, means
                                  there is no specification for
                                  lifetime.  Extensions should feel
                                  free to ignore this parameter. */
} afs_msg_klog;

/*!@}*/

/*!@}*/

#endif
