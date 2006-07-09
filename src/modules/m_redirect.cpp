/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *           	  <Craig@chatspike.net>
 *     
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

using namespace std;

#include <stdio.h>
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "helperfuncs.h"

/* $ModDesc: Provides channel mode +L (limit redirection) */

class Redirect : public ModeHandler
{
	Server* Srv;
 public:
	Redirect(Server* s) : ModeHandler('L', 1, 0, false, MODETYPE_CHANNEL, false), Srv(s) { }

	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding)
	{
		if (adding)
		{
			chanrec* c = NULL;

			if (!IsValidChannelName(parameter.c_str()))
			{
				WriteServ(source->fd,"403 %s %s :Invalid channel name",source->nick, parameter.c_str());
				parameter = "";
				return MODEACTION_DENY;
			}

			c = Srv->FindChannel(parameter);
			if (c)
			{
				/* Fix by brain: Dont let a channel be linked to *itself* either */
				if ((c == channel) || (c->IsModeSet('L')))
				{
					WriteServ(source->fd,"690 %s :Circular redirection, mode +L to %s not allowed.",source->nick,parameter.c_str());
					parameter = "";
					return MODEACTION_DENY;
				}
			}

			channel->SetMode('L', true);
			channel->SetModeParam('L', parameter.c_str(), true);
			return MODEACTION_ALLOW;
		}
		else
		{
			if (channel->IsModeSet('L'))
			{
				channel->SetMode('L', false);
				return MODEACTION_ALLOW;
			}
		}

		return MODEACTION_DENY;
		
	}
};

class ModuleRedirect : public Module
{
	Server *Srv;
	Redirect* re;
	
 public:
 
	ModuleRedirect(Server* Me)
		: Module::Module(Me)
	{
		Srv = Me;
		re = new Redirect(Me);
		Srv->AddMode(re, 'L');
	}
	
	void Implements(char* List)
	{
		List[I_On005Numeric] = List[I_OnUserPreJoin] = 1;
	}

	virtual void On005Numeric(std::string &output)
	{
		InsertMode(output, "L", 3);
	}
	
	virtual int OnUserPreJoin(userrec* user, chanrec* chan, const char* cname)
	{
		if (chan)
		{
			if (chan->IsModeSet('L'))
			{
				if (Srv->CountUsers(chan) >= chan->limit)
				{
					std::string channel = chan->GetModeParameter('L');
					WriteServ(user->fd,"470 %s :%s has become full, so you are automatically being transferred to the linked channel %s",user->nick,cname,channel.c_str());
					Srv->JoinUserToChannel(user,channel.c_str(),"");
					return 1;
				}
			}
		}
		return 0;
	}

	virtual ~ModuleRedirect()
	{
		DELETE(re);
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,0,VF_STATIC|VF_VENDOR);
	}
};


class ModuleRedirectFactory : public ModuleFactory
{
 public:
	ModuleRedirectFactory()
	{
	}
	
	~ModuleRedirectFactory()
	{
	}
	
	virtual Module * CreateModule(Server* Me)
	{
		return new ModuleRedirect(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleRedirectFactory;
}

