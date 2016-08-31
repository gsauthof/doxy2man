#ifndef OMG_H
#define OMG_H


/** \file omg.h
 *
 * \brief OMG interface library.
 *
 * Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed
 * do eiusmod tempor incididunt ut labore et dolore magna aliqua.
 * Ut enim ad minim veniam, quis nostrud exercitation ullamco
 * laboris nisi ut aliquip ex ea commodo consequat. Duis aute
 * irure dolor in reprehenderit in voluptate velit esse cillum
 * dolore eu fugiat nulla pariatur. Excepteur sint occaecat
 * cupidatat non proident, sunt in culpa qui officia deserunt
 * mollit anim id est laborum.
 *
 * \copyright Acme Corporation
 *
 */

struct OMG_Creds {
  char uid[32]; /**< \brief user identifier */
  char token[64];
};
struct OMG_Connection;

/** \brief Connect to an omg object.
 *
 * Usually first library function to call.
 *
 * \retval 0 error
 * \retval other opaque connection object
 *
 * \author Joe User <juser@example.org>
 */
struct OMG_Connection *omg_connect(const struct OMG_Creds *cred,
    int *error_code);


/** \brief Retrieve message of the day.
 *
 *
 * Non eram nescius, Brute, cum, quae summis ingeniis
 * exquisitaque doctrina philosophi Graeco sermone tractavissent,
 * ea Latinis litteris mandaremus, fore ut hic noster labor in
 * varias reprehensiones incurreret. nam quibusdam, et iis quidem
 * non admodum indoctis, totum hoc displicet philosophari. quidam
 * autem non tam id reprehendunt, si remissius agatur, sed tantum
 * studium tamque multam operam ponendam in eo non arbitrantur.
 * erunt etiam, et ii quidem eruditi Graecis litteris,
 * contemnentes Latinas, qui se dicant in Graecis legendis operam
 * malle consumere. postremo aliquos futuros suspicor, qui me ad
 * alias litteras vocent, genus hoc scribendi, etsi sit elegans,
 * personae tamen et dignitatis esse negent.
 *
 * \param c connection handle
 * \param funkyness greater value means more absurdity
 *
 * \sa free(3)
 *
 * \copyright Random Dude
 *
 * \retval 0 error
 * \retval other the message
*/
char *omg_motd(OMG_Connection *c, int funkyness,
    int *error_code /**< \brief optional. */);



/** \brief Disconnect and free resources.
 *
 * Usually last library function to call.
 *
 * \sa omg_connect
 *
 * \retval 0 success
 * \retval other error codes
 *
 * \author Joe User <juser@example.org>
 */
int omg_disconnect(struct OMG_Connection *c);

#endif
