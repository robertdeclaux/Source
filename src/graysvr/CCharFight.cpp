//
// CCharFight.cpp
// Copyright Menace Software (www.menasoft.com).
//
// Fight/Criminal actions/Noto.
//

#include "graysvr.h"	// predef header.
#include "CClient.h"
#include "../network/send.h"

CItemStone * CChar::Guild_Find( MEMORY_TYPE MemType ) const
{
	ADDTOCALLSTACK("CChar::Guild_Find");
	// Get my guild stone for my guild. even if i'm just a STONEPRIV_CANDIDATE ?
	// ARGS:
	//  MemType == MEMORY_GUILD or MEMORY_TOWN
	if ( ! m_pPlayer )
		return( NULL );
	CItemMemory * pMyGMem = Memory_FindTypes( MemType );
	if ( ! pMyGMem )
		return( NULL );
	CItemStone * pMyStone = dynamic_cast <CItemStone*>( pMyGMem->m_uidLink.ItemFind());
	if ( pMyStone == NULL )
	{
		// Some sort of mislink ! fix it.
		const_cast <CChar*>(this)->Memory_ClearTypes( MemType ); 	// Make them forget they were ever in this guild....again!
		return( NULL );
	}
	return( pMyStone );
}

CStoneMember * CChar::Guild_FindMember( MEMORY_TYPE MemType ) const
{
	ADDTOCALLSTACK("CChar::Guild_FindMember");
	// Get my member record for my guild.
	CItemStone * pMyStone = Guild_Find(MemType);
	if ( pMyStone == NULL )
		return( NULL );
	CStoneMember * pMember = pMyStone->GetMember( this );
	if ( pMember == NULL )
	{
		// Some sort of mislink ! fix it.
		const_cast <CChar*>(this)->Memory_ClearTypes( MemType ); 	// Make them forget they were ever in this guild....again!
		return( NULL );
	}
	return( pMember );
}

void CChar::Guild_Resign( MEMORY_TYPE MemType )
{
	ADDTOCALLSTACK("CChar::Guild_Resign");
	// response to "I resign from my guild" or "town"
	// Are we in an active battle ?

	if ( IsStatFlag( STATF_DEAD ))
		return;

	CStoneMember * pMember = Guild_FindMember(MemType);
	if ( pMember == NULL )
		return ;

	if ( pMember->IsPrivMember())
	{
		CItemMemory * pMemFight = Memory_FindTypes( MEMORY_FIGHT );
		if ( pMemFight )
		{
			CItemStone * pMyStone = pMember->GetParentStone();
			ASSERT(pMyStone);
			SysMessagef( g_Cfg.GetDefaultMsg( DEFMSG_GUILDRESIGN ), static_cast<LPCTSTR>(pMyStone->GetTypeName()) );
		}
	}

	delete pMember;
}

LPCTSTR CChar::Guild_Abbrev( MEMORY_TYPE MemType ) const
{
	ADDTOCALLSTACK("CChar::Guild_Abbrev");
	// Get my guild abbrev if i have chosen to turn it on.
	CStoneMember * pMember = Guild_FindMember(MemType);
	if ( pMember == NULL )
		return( NULL );
	if ( ! pMember->IsAbbrevOn())
		return( NULL );
	CItemStone * pMyStone = pMember->GetParentStone();
	if ( pMyStone == NULL ||
		! pMyStone->GetAbbrev()[0] )
		return( NULL );
	return( pMyStone->GetAbbrev());
}

LPCTSTR CChar::Guild_AbbrevBracket( MEMORY_TYPE MemType ) const
{
	ADDTOCALLSTACK("CChar::Guild_AbbrevBracket");
	// Get my [guild abbrev] if i have chosen to turn it on.
	LPCTSTR pszAbbrev = Guild_Abbrev(MemType);
	if ( pszAbbrev == NULL )
		return( NULL );
	TCHAR * pszTemp = Str_GetTemp();
	sprintf( pszTemp, " [%s]", pszAbbrev );
	return( pszTemp );
}

//*****************************************************************

bool CChar::Noto_IsMurderer() const
{
	ADDTOCALLSTACK("CChar::Noto_IsMurderer");
	return( m_pPlayer && m_pPlayer->m_wMurders > g_Cfg.m_iMurderMinCount );
}

bool CChar::Noto_IsEvil() const
{
	ADDTOCALLSTACK("CChar::Noto_IsEvil");
	int		iKarma = Stat_GetAdjusted(STAT_KARMA);

	//	guarded areas could be both RED and BLUE ones.
	if ( m_pArea && m_pArea->IsGuarded() && m_pArea->m_TagDefs.GetKeyNum("RED", true) )
	{
		//	red zone is opposite to blue - murders are considered normal here
		//	while people with 0 kills and good karma are considered bad here

		if ( Noto_IsMurderer() )
			return false;

		if ( m_pPlayer )
		{
			if ( iKarma > g_Cfg.m_iPlayerKarmaEvil )
				return true;
		}
		else
		{
			if ( iKarma > 0 )
				return true;
		}

		return false;
	}

	// animals and humans given more leeway.
	if ( Noto_IsMurderer() )
		return true;
	switch ( GetNPCBrain() )
	{
		case NPCBRAIN_UNDEAD:
		case NPCBRAIN_MONSTER:
			return ( iKarma< 0 );
		case NPCBRAIN_BERSERK:
			return true;
		case NPCBRAIN_ANIMAL:
			return ( iKarma<= -800 );
		default:
			break;
	}
	if ( m_pPlayer )
	{
		return ( iKarma < g_Cfg.m_iPlayerKarmaEvil );
	}
	return ( iKarma <= -3000 );
}

bool CChar::Noto_IsNeutral() const
{
	ADDTOCALLSTACK("CChar::Noto_IsNeutral");
	// Should neutrality change in guarded areas ?
	int iKarma = Stat_GetAdjusted(STAT_KARMA);
	switch ( GetNPCBrain() )
	{
		case NPCBRAIN_MONSTER:
		case NPCBRAIN_BERSERK:
			return( iKarma<= 0 );
		case NPCBRAIN_ANIMAL:
			return( iKarma<= 100 );
		default:
			break;
	}
	if ( m_pPlayer )
	{
		return( iKarma<g_Cfg.m_iPlayerKarmaNeutral );
	}
	return( iKarma<0 );
}

NOTO_TYPE CChar::Noto_GetFlag( const CChar * pCharViewer, bool fAllowIncog, bool fAllowInvul ) const
{
	ADDTOCALLSTACK("CChar::Noto_GetFlag");
	// What is this char to the viewer ?
	// This allows the noto attack check in the client.
	// NOTO_GOOD = it is criminal to attack me.
	CChar * pThis = const_cast<CChar*>(this);
	CChar * pTarget = const_cast<CChar*>(pCharViewer);
	NOTO_TYPE Noto;
	if ( pThis->m_notoSaves.size() )
	{
		int id = pThis->NotoSave_GetID( pTarget );
		if ( id != -1 )
		{
			return pThis->NotoSave_GetValue( id );
		}
	}
	if (IsTrigUsed(TRIGGER_NOTOSEND))
	{
		CScriptTriggerArgs args;
		pThis->OnTrigger(CTRIG_NotoSend, pTarget, &args);
		Noto = static_cast<NOTO_TYPE>(args.m_iN1);
		if (Noto != NOTO_INVALID )
		{
			pThis->NotoSave_Add( pTarget, Noto);
			return Noto;
		}
	}
	Noto = Noto_CalcFlag( pCharViewer, fAllowIncog, fAllowInvul);
	pThis->NotoSave_Add(pTarget, Noto);
	return Noto;
}

NOTO_TYPE CChar::Noto_CalcFlag( const CChar * pCharViewer, bool fAllowIncog, bool fAllowInvul ) const
{
	ADDTOCALLSTACK("CChar::Noto_CalcFlag");
	NOTO_TYPE iNotoFlag = static_cast<NOTO_TYPE>(m_TagDefs.GetKeyNum("OVERRIDE.NOTO", true));
	if ( iNotoFlag != NOTO_INVALID )
		return iNotoFlag;

	if ( fAllowIncog && IsStatFlag( STATF_Incognito ))
	{
		return NOTO_NEUTRAL;
	}

	if ( fAllowInvul && IsStatFlag( STATF_INVUL ) )
	{
		return NOTO_INVUL;
	}

	if ( this != pCharViewer )
	{
		if ( g_Cfg.m_iPetsInheritNotoriety != 0 )
		{
			// Do we have a master to inherit notoriety from?
			CChar* pMaster = NPC_PetGetOwner();
			if (pMaster != NULL && pMaster != pCharViewer) // master doesn't want to see their own status
			{
				// protect against infinite loop
				static int sm_iReentrant = 0;
				if (sm_iReentrant < 32)
				{
					// return master's notoriety
					++sm_iReentrant;
					NOTO_TYPE notoMaster = pMaster->Noto_GetFlag(pCharViewer, fAllowIncog, fAllowInvul);
					--sm_iReentrant;

					// check if notoriety is inheritable based on bitmask setting:
					//		NOTO_GOOD		= 0x01
					//		NOTO_GUILD_SAME	= 0x02
					//		NOTO_NEUTRAL	= 0x04
					//		NOTO_CRIMINAL	= 0x08
					//		NOTO_GUILD_WAR	= 0x10
					//		NOTO_EVIL		= 0x20
					//		NOTO_INVUL		= 0x40
					int iNotoFlag = 1 << (notoMaster - 1);
					if ( (g_Cfg.m_iPetsInheritNotoriety & iNotoFlag) == iNotoFlag )
						return notoMaster;
				}
				else
				{
					DEBUG_ERR(("Too many owners (circular ownership?) to continue acquiring notoriety towards %s uid=0%lx\n", pMaster->GetName(), pMaster->GetUID().GetPrivateUID()));
					// too many owners, return the notoriety for however far we got down the chain
				}
			}
		}

		// Are we in the same party ?
		if ( m_pParty && m_pParty == pCharViewer->m_pParty )
		{
			if ( m_pParty->GetLootFlag(this))
			{
				return(NOTO_GUILD_SAME);
			}
			return(NOTO_GUILD_SAME);	// Shall we be green
		}
	}

	if ( Noto_IsEvil())
	{
		return( NOTO_EVIL );
	}

	if ( this != pCharViewer ) // Am I checking myself?
	{
		// Check the guild stuff
		CItemStone * pMyTown = Guild_Find(MEMORY_TOWN);
		CItemStone * pMyGuild = Guild_Find(MEMORY_GUILD);
		if ( pMyGuild || pMyTown )
		{
			CItemStone * pViewerGuild = pCharViewer->Guild_Find(MEMORY_GUILD);
			CItemStone * pViewerTown = pCharViewer->Guild_Find(MEMORY_TOWN);
			// Are we both in a guild?
			if ( pViewerGuild || pViewerTown )
			{
				if ( pMyGuild && pMyGuild->IsPrivMember(this))
				{
					if ( pViewerGuild && pViewerGuild->IsPrivMember(pCharViewer))
					{
						if ( pViewerGuild == pMyGuild ) // Same guild?
							return NOTO_GUILD_SAME; // return green
						if ( pMyGuild->IsAlliedWith(pViewerGuild))
							return NOTO_GUILD_SAME;
						// Are we in different guilds but at war? (not actually a crime right?)
						if ( pMyGuild->IsAtWarWith(pViewerGuild))
							return NOTO_GUILD_WAR; // return orange
					}
					if ( pMyGuild->IsAtWarWith(pViewerTown))
						return NOTO_GUILD_WAR; // return orange
				}
				if ( pMyTown && pMyTown->IsPrivMember(this))
				{
					if ( pViewerGuild && pViewerGuild->IsPrivMember(pCharViewer))
					{
						if ( pMyTown->IsAtWarWith(pViewerGuild))
							return NOTO_GUILD_WAR; // return orange
					}
					if ( pMyTown->IsAtWarWith(pViewerTown))
						return NOTO_GUILD_WAR; // return orange
				}
			}
		}
	}

	if ( IsStatFlag( STATF_Criminal ))	// criminal to everyone.
	{
		return( NOTO_CRIMINAL );
	}

	if ( this != pCharViewer ) // Am I checking myself?
	{
		if ( NPC_IsOwnedBy( pCharViewer, false ))	// All pets are neutral to their owners.
			return( NOTO_NEUTRAL );

		// If they saw me commit a crime or I am their aggressor then
		// criminal to just them.
		CItemMemory * pMemory = pCharViewer->Memory_FindObjTypes( this, MEMORY_SAWCRIME | MEMORY_AGGREIVED );
		if ( pMemory != NULL )
		{
			return( NOTO_CRIMINAL );
		}
	}

	if ( m_pArea && m_pArea->IsFlag(REGION_FLAG_ARENA))
	{
		// everyone is neutral here.
		return( NOTO_NEUTRAL );
	}

	if ( Noto_IsNeutral() || m_TagDefs.GetKeyNum("NOTO.PERMAGREY", true))
	{
		return( NOTO_NEUTRAL );
	}

	return( NOTO_GOOD );
}

HUE_TYPE CChar::Noto_GetHue( const CChar * pCharViewer, bool fIncog ) const
{
	ADDTOCALLSTACK("CChar::Noto_GetHue");
	// What is this char to the viewer ?
	// Represent as a text Hue.
	CVarDefCont * sVal = GetKey( "NAME.HUE", true );
	if ( sVal )
		return  static_cast<HUE_TYPE>(sVal->GetValNum());

	switch ( Noto_GetFlag( pCharViewer, fIncog, true ))
	{
		case NOTO_GOOD:			return g_Cfg.m_iColorNotoGood;		// Blue
		case NOTO_GUILD_SAME:	return g_Cfg.m_iColorNotoGuildSame; // Green (same guild)
		case NOTO_NEUTRAL:		return g_Cfg.m_iColorNotoNeutral;	// Grey 1 (someone that can be attacked)
		case NOTO_CRIMINAL:		return g_Cfg.m_iColorNotoCriminal;	// Grey 2 (criminal)
		case NOTO_GUILD_WAR:	return g_Cfg.m_iColorNotoGuildWar;	// Orange (enemy guild)
		case NOTO_INVUL:		return g_Cfg.m_iColorNotoInvul;		// Yellow
		case NOTO_EVIL:			return g_Cfg.m_iColorNotoEvil;		// Red
		default:				return g_Cfg.m_iColorNotoDefault;	// Grey
	}
}

LPCTSTR CChar::Noto_GetFameTitle() const
{
	ADDTOCALLSTACK("CChar::Noto_GetFameTitle");
	if ( IsStatFlag(STATF_Incognito|STATF_Polymorph) )
		return "";

	if ( !IsPriv(PRIV_PRIV_NOSHOW) )	// PRIVSHOW is on
	{
		if ( IsPriv(PRIV_GM) )			// GM mode is on
		{
			switch ( GetPrivLevel() )
			{
				case PLEVEL_Owner:
					return g_Cfg.GetDefaultMsg( DEFMSG_TITLE_OWNER );	//"Owner ";
				case PLEVEL_Admin:
					return g_Cfg.GetDefaultMsg( DEFMSG_TITLE_ADMIN );	//"Admin ";
				case PLEVEL_Dev:
					return g_Cfg.GetDefaultMsg( DEFMSG_TITLE_DEV );	//"Dev ";
				case PLEVEL_GM:
					return g_Cfg.GetDefaultMsg( DEFMSG_TITLE_GM );	//"GM ";
				default:
					break;
			}
		}
		switch ( GetPrivLevel() )
		{
			case PLEVEL_Seer: return g_Cfg.GetDefaultMsg( DEFMSG_TITLE_SEER );	//"Seer ";
			case PLEVEL_Counsel: return g_Cfg.GetDefaultMsg( DEFMSG_TITLE_COUNSEL );	//"Counselor ";
			default: break;
		}
	}

	if (( Stat_GetAdjusted(STAT_FAME) > 9900 ) && (m_pPlayer || !g_Cfg.m_NPCNoFameTitle))
		return Char_GetDef()->IsFemale() ? g_Cfg.GetDefaultMsg( DEFMSG_TITLE_LADY ) : g_Cfg.GetDefaultMsg( DEFMSG_TITLE_LORD );	//"Lady " : "Lord ";

	return "";
}

int CChar::Noto_GetLevel() const
{
	ADDTOCALLSTACK("CChar::Noto_GetLevel");
	// Paperdoll title for character
	// This is so we can inform user of change in title !

	size_t i = 0;
	int iKarma = Stat_GetAdjusted(STAT_KARMA);
	for ( ; i < g_Cfg.m_NotoKarmaLevels.GetCount() && iKarma < g_Cfg.m_NotoKarmaLevels.GetAt(i); i++ )
		;

	size_t j = 0;
	int iFame = Stat_GetAdjusted(STAT_FAME);
	for ( ; j < g_Cfg.m_NotoFameLevels.GetCount() && iFame > g_Cfg.m_NotoFameLevels.GetAt(j); j++ )
		;

	return( ( i * (g_Cfg.m_NotoFameLevels.GetCount() + 1) ) + j );
}

LPCTSTR CChar::Noto_GetTitle() const
{
	ADDTOCALLSTACK("CChar::Noto_GetTitle");
	// Paperdoll title for character

	LPCTSTR pTitle = Noto_IsMurderer() ? g_Cfg.GetDefaultMsg( DEFMSG_TITLE_MURDERER ) : ( IsStatFlag(STATF_Criminal) ? g_Cfg.GetDefaultMsg( DEFMSG_TITLE_CRIMINAL ) :  g_Cfg.GetNotoTitle(Noto_GetLevel(), Char_GetDef()->IsFemale()) );
	LPCTSTR pFameTitle = GetKeyStr("NAME.PREFIX");
	if ( !*pFameTitle )
		pFameTitle = Noto_GetFameTitle();

	TCHAR * pTemp = Str_GetTemp();
	sprintf( pTemp, "%s%s%s%s%s%s",
		(pTitle[0]) ? ( Char_GetDef()->IsFemale() ? g_Cfg.GetDefaultMsg( DEFMSG_TITLE_ARTICLE_FEMALE ) : g_Cfg.GetDefaultMsg( DEFMSG_TITLE_ARTICLE_MALE ) )  : "",
		pTitle,
		(pTitle[0]) ? " " : "",
		pFameTitle,
		GetName(),
		GetKeyStr("NAME.SUFFIX"));

	return pTemp;
}

void CChar::Noto_Murder()
{
	ADDTOCALLSTACK("CChar::Noto_Murder");
	// I am a murderer (it seems) (update my murder decay item)
	if ( Noto_IsMurderer() )
		SysMessageDefault(DEFMSG_MURDERER);

	if ( m_pPlayer && m_pPlayer->m_wMurders )
		Spell_Effect_Create(SPELL_NONE, LAYER_FLAG_Murders, 0, g_Cfg.m_iMurderDecayTime, NULL);
}

bool CChar::Noto_Criminal( CChar * pChar)
{
	ADDTOCALLSTACK("CChar::Noto_Criminal");
	// I am a criminal and the guards will be on my ass.
	if ( IsPriv(PRIV_GM))
		return false;
	int decay = g_Cfg.m_iCriminalTimer;
	CScriptTriggerArgs Args;
	Args.m_iN1 = decay;
	if ( IsTrigUsed(TRIGGER_CRIMINAL) )
	{
		Args.m_pO1 = pChar;
		if ( ( OnTrigger( CTRIG_Criminal, this, &Args ) ) == TRIGRET_RET_TRUE )
			return false;

		decay = static_cast<int>(Args.m_iN1);
	}
	if ( !IsStatFlag( STATF_Criminal) ) SysMessageDefault( DEFMSG_CRIMINAL );
	Spell_Effect_Create(SPELL_NONE, LAYER_FLAG_Criminal, 0, decay, NULL);
	NotoSave_Update();
	return true;
}

void CChar::Noto_ChangeDeltaMsg( int iDelta, LPCTSTR pszType )
{
	ADDTOCALLSTACK("CChar::Noto_ChangeDeltaMsg");
	if ( !iDelta )
		return;

#define	NOTO_DEGREES	8
#define	NOTO_FACTOR		(300/NOTO_DEGREES)

	static UINT const sm_DegreeTable[8] =
	{
		DEFMSG_NOTO_CHANGE_1,
		DEFMSG_NOTO_CHANGE_2,
		DEFMSG_NOTO_CHANGE_3,
		DEFMSG_NOTO_CHANGE_4,
		DEFMSG_NOTO_CHANGE_5,
		DEFMSG_NOTO_CHANGE_6,
		DEFMSG_NOTO_CHANGE_7,
		DEFMSG_NOTO_CHANGE_8		// 300 = huge
	};

	int iDegree = minimum(abs(iDelta) / NOTO_FACTOR, 7);

	TCHAR *pszMsg = Str_GetTemp();
	sprintf( pszMsg, g_Cfg.GetDefaultMsg( DEFMSG_NOTO_CHANGE_0 ), 
		( iDelta < 0 ) ? g_Cfg.GetDefaultMsg( DEFMSG_NOTO_CHANGE_LOST ) : g_Cfg.GetDefaultMsg( DEFMSG_NOTO_CHANGE_GAIN ),
		 g_Cfg.GetDefaultMsg(sm_DegreeTable[iDegree]), pszType );
	
	SysMessage( pszMsg );
}

void CChar::Noto_ChangeNewMsg( int iPrvLevel )
{
	ADDTOCALLSTACK("CChar::Noto_ChangeNewMsg");
	if ( iPrvLevel != Noto_GetLevel())
	{
		// reached a new title level ?
		SysMessagef( g_Cfg.GetDefaultMsg( DEFMSG_NOTO_GETTITLE ), static_cast<LPCTSTR>(Noto_GetTitle()));
	}
}

void CChar::Noto_Fame( int iFameChange )
{
	ADDTOCALLSTACK("CChar::Noto_Fame");
	// Fame should only go down on death, time or cowardice ?

	if ( ! iFameChange )
		return;

	int iFame = maximum(Stat_GetAdjusted(STAT_FAME), 0);

	iFameChange = g_Cfg.Calc_FameScale( iFame, iFameChange );

	if ( IsTrigUsed(TRIGGER_FAMECHANGE) )
	{
		CScriptTriggerArgs Args(iFameChange);	// ARGN1 - Fame change modifier
		TRIGRET_TYPE retType = OnTrigger(CTRIG_FameChange, this, &Args);

		if ( retType == TRIGRET_RET_TRUE )
			return;
		iFameChange = static_cast<int>(Args.m_iN1);
	}
	
	if ( ! iFameChange  )
		return;

	iFame += iFameChange;
	if ( iFame < 0 )
		iFame = 0;
	if ( iFame > g_Cfg.m_iMaxFame)
		iFame = g_Cfg.m_iMaxFame; // Maximum reached

	Noto_ChangeDeltaMsg( iFame - Stat_GetAdjusted(STAT_FAME), g_Cfg.GetDefaultMsg( DEFMSG_NOTO_FAME ) );
	Stat_SetBase(STAT_FAME,iFame);
}

void CChar::Noto_Karma( int iKarmaChange, int iBottom, bool bMessage )
{
	ADDTOCALLSTACK("CChar::Noto_Karma");
	// iBottom is a variable where you control at what point
	// the loss for this action stop (as in stealing shouldnt
	// take you to dread ). iBottom def. to g_Cfg.m_iMinKarma if you leave
	// it out.

	if ( iKarmaChange < 1 )
		return;
	if (iBottom == INT_MIN)
		iBottom = g_Cfg.m_iMinKarma;

	int	iKarma = Stat_GetAdjusted(STAT_KARMA);

	iKarmaChange = g_Cfg.Calc_KarmaScale( iKarma, iKarmaChange );

	if ( IsTrigUsed(TRIGGER_KARMACHANGE) )
	{
		CScriptTriggerArgs Args(iKarmaChange);	// ARGN1 - Karma change modifier
		TRIGRET_TYPE retType = OnTrigger(CTRIG_KarmaChange, this, &Args);

		if ( retType == TRIGRET_RET_TRUE )
			return;
		iKarmaChange = static_cast<int>(Args.m_iN1);
	}
	
	if ( iKarmaChange < 1 )
		return;

	// If we are going to loose karma and are already below bottom
	// then return.
	if (( iKarma <= iBottom ) && ( iKarmaChange < 0 ))
		return;

	iKarma += iKarmaChange;
	if ( iKarmaChange < 0 )
	{
		if ( iKarma < iBottom )
			iKarma = iBottom;
	}
	else
	{
		if ( iKarma > g_Cfg.m_iMaxKarma )
			iKarma = g_Cfg.m_iMaxKarma;
	}

	Noto_ChangeDeltaMsg( iKarma - Stat_GetAdjusted(STAT_KARMA), g_Cfg.GetDefaultMsg( DEFMSG_NOTO_KARMA ) );
	Stat_SetBase(STAT_KARMA,iKarma);
	NotoSave_Update();
	if ( bMessage == true)
	{
		int iPrvLevel = Noto_GetLevel();
		Noto_ChangeNewMsg( iPrvLevel );
	}
}

void CChar::Noto_KarmaChangeMessage( int iKarmaChange, int iLimit )
{
	ADDTOCALLSTACK("CChar::Noto_KarmaChangeMessage");
	// Change your title ?
	Noto_Karma( iKarmaChange, iLimit, true );
}

extern unsigned int Calc_ExpGet_Exp(unsigned int);

void CChar::Noto_Kill(CChar * pKill, bool fPetKill, int iOtherKillers)
{
	ADDTOCALLSTACK("CChar::Noto_Kill");
	// I participated in killing pKill CChar. (called from Death())
	// I Get some fame/karma. (maybe)

	if ( !pKill )
		return;

	// What was there noto to me ?
	NOTO_TYPE NotoThem = pKill->Noto_GetFlag( this, false );

	// Fight is over now that i have won. (if i was fighting at all )
	// ie. Magery cast might not be a "fight"
	Fight_Clear( pKill );
	if ( pKill == this )
	{
		return;
	}

	if ( !m_pPlayer )
	{
		ASSERT( m_pNPC );

		// I am a guard ?
		if ( ! pKill->m_pPlayer &&
			m_pNPC->m_Brain == NPCBRAIN_GUARD )
		{
			// Don't create a corpse or loot if NPC killed by a guard.
			// No corpse and no loot !
			pKill->StatFlag_Set( STATF_Conjured );
			return;
		}

		// Check to see if anything is on the corpse I just killed
		//Skill_Start( NPCACT_LOOKING );	// look around for loot.
		Skill_Start(NPCACT_LOOTING);	// once the npc starts looking it gets trapped in a loop - This appears to work and the npc no longer stays frozen
		// If an NPC kills an NPC then it doesn't count.
		if ( pKill->m_pNPC )
			return;
	}
	else if ( NotoThem < NOTO_NEUTRAL && NotoThem != NOTO_GUILD_SAME )
	{
		ASSERT( m_pPlayer );
		// I'm a murderer !
		if ( ! IsPriv(PRIV_GM))
		{
			CScriptTriggerArgs args;
			args.m_iN1 = m_pPlayer->m_wMurders+1;
			args.m_iN2 = true;

			if ( IsTrigUsed(TRIGGER_MURDERMARK) )
			{
				OnTrigger(CTRIG_MurderMark, this, &args);
				if ( args.m_iN1 < 0 )
					args.m_iN1 = 0;
			}

			m_pPlayer->m_wMurders = static_cast<WORD>(args.m_iN1);
			NotoSave_Update();
			if ( args.m_iN2 ) 
				Noto_Criminal();

			Noto_Murder();
		}
	}

	if ( NotoThem != NOTO_GUILD_SAME )			// no fame/karma gain for killing guildmates
	{
		// Store current notolevel before changes, used to check if notlvl is changed.
		int iPrvLevel = Noto_GetLevel();
		int iFameChange = g_Cfg.Calc_FameKill(pKill) / (iOtherKillers + 1);
		int iKarmaChange = g_Cfg.Calc_KarmaKill(pKill, NotoThem) / (iOtherKillers + 1);

		Noto_Karma(iKarmaChange);

		// no real fame for letting your pets do the work! and killing summoned pets as well
		if ( !fPetKill && pKill->IsStatFlag(STATF_Conjured) )
			fPetKill = true;
	
		if ( !fPetKill )							
		{
			Noto_Fame(iFameChange);

			//	count change of experience
			if ( g_Cfg.m_bExperienceSystem && ( g_Cfg.m_iExperienceMode&EXP_MODE_RAISE_COMBAT ))
			{
				// default delta = exp/10 (same as death loss), divided for each killer proportionaly
				int change = pKill->m_exp;

				if ( g_Cfg.m_bLevelSystem &&
					( (g_Cfg.m_iExperienceMode & (EXP_MODE_ALLOW_DOWN|EXP_MODE_DOWN_NOLEVEL)) == (EXP_MODE_ALLOW_DOWN|EXP_MODE_DOWN_NOLEVEL) ) &&
					pKill->m_pPlayer )
				{
					//	to disallow players to get some level and then allow kill self without
					//	any bad affect on self - share only exp till the current level
					change -= Calc_ExpGet_Exp(pKill->m_level);
					if ( change < 0 )
						change = 0;
				}

				change = (change/10)/(iOtherKillers + 1);

				if ( change )							// PvP / PvM ?
				{
					// NPC is considering self as a player if being a killer.
					if ( pKill->m_pPlayer )
						change = ( change * g_Cfg.m_iExperienceKoefPVP )/100;
					else
						change = ( change * g_Cfg.m_iExperienceKoefPVM )/100;
				}

				if ( change )
				{
					//	bonuses of different experiences
					if ( (m_exp * 4) < pKill->m_exp )		// 200%		[exp = 1/4 of killed]
						change *= 2;
					else if ( (m_exp * 2) < pKill->m_exp )	// 150%		[exp = 1/2 of killed]
						change = (change * 3) / 2;
					else if ( m_exp <= pKill->m_exp )		// 100%		[exp <= killed]
						;
					else if ( m_exp < (pKill->m_exp * 2) )	//  50%		[exp < 2 * killed]
						change /= 2;
					else if ( m_exp < (pKill->m_exp * 3) )	//  25%		[exp < 3 * killed]
						change /= 4;
					else									//  10%		[exp >= 3 * killed]
						change /= 10;
				}

				//	bonuses of different experience levels
				if ( change && g_Cfg.m_bLevelSystem && pKill->m_pPlayer )
				{
					int dlevel = m_level - pKill->m_level;

					if ( dlevel < 0 )				//	+10% per each lower level
						change = (change * (100 + 10 * (minimum(abs(dlevel),10))))/100;
					else if ( !dlevel )
						;
					else							//	-10% per each upper level
						change = (change * (100 - 10 * (minimum(dlevel,10))))/100;
				}

				ChangeExperience(change, pKill);
			}
		}

		Noto_ChangeNewMsg( iPrvLevel ); // Inform on any notlvl changes.
	}
}

void CChar::NotoSave_Add( CChar * pChar, NOTO_TYPE value )
{
	ADDTOCALLSTACK("CChar::NotoSave_Add");
	if ( !pChar )
		return;
	CGrayUID uid = static_cast<CGrayUID>(pChar->GetUID());
	if  ( m_notoSaves.size() )	// Must only check for existing attackers if there are any attacker already.
	{
		for (std::vector<NotoSaves>::iterator it = m_notoSaves.begin(); it != m_notoSaves.end(); ++it)
		{
			NotoSaves & refNoto = *it;
			if ( refNoto.charUID == uid )
			{
				//Found one, no actions needed so we skip
				return;
			}
		}
	}
	NotoSaves refNoto;
	refNoto.value = value;
	refNoto.charUID = pChar->GetUID();
	refNoto.time = 0;
	m_notoSaves.push_back(refNoto);
}
	
NOTO_TYPE CChar::NotoSave_GetValue( int id )
{
	ADDTOCALLSTACK("CChar::NotoSave_GetValue");
	if ( !m_notoSaves.size() )
		return NOTO_INVALID;
	if ( id < 0 )
		return NOTO_INVALID;
	if ( static_cast<int>(m_notoSaves.size()) <= id )
		return NOTO_INVALID;
	NotoSaves & refNotoSave = m_notoSaves.at(id);
	return refNotoSave.value;
}

INT64 CChar::NotoSave_GetTime( int id )
{
	ADDTOCALLSTACK("CChar::NotoSave_GetTime");
	if ( !m_notoSaves.size() )
		return -1;
	if ( id < 0 )
		return NOTO_INVALID;
	if ( static_cast<int>(m_notoSaves.size()) <= id )
		return -1;
	NotoSaves & refNotoSave = m_notoSaves.at(id);
	return refNotoSave.time;
}

void CChar::NotoSave_SetValue( CChar * pChar, NOTO_TYPE value )
{
	ADDTOCALLSTACK("CChar::NotoSave_SetValue(CChar)");
	if ( !m_notoSaves.size() )
		return;
	if ( ! pChar )
		return;
	int id = NotoSave_GetID(pChar);
	if ( id < 0 )
		return;
	NotoSaves & refNotoSave = m_notoSaves.at( id );
	refNotoSave.value = value;
}

void CChar::NotoSave_SetValue( int pChar, NOTO_TYPE value)
{
	ADDTOCALLSTACK("CChar::NotoSave_SetValue(int)");
	if ( !m_notoSaves.size() )
		return;
	if ( !pChar )
		return;
	if ( static_cast<int>(m_notoSaves.size()) <= pChar )
		return;
	NotoSaves & refNotoSave = m_notoSaves.at( pChar );
	refNotoSave.value = value;
}

void CChar::NotoSave_Clear()
{
	ADDTOCALLSTACK("CChar::NotoSave_Clear");
	if ( m_notoSaves.size() )
		m_notoSaves.clear();
}

void CChar::NotoSave_Update()
{
	ADDTOCALLSTACK("CChar::NotoSave_Clear");
	NotoSave_Clear();
	UpdateMode( NULL , false );
	ResendTooltip();
}

void CChar::NotoSave_CheckTimeout()
{
	ADDTOCALLSTACK("CChar::NotoSave_CheckTimeout");
	if (m_notoSaves.size())
	{
		int count = 0;
		for (std::vector<NotoSaves>::iterator it = m_notoSaves.begin(); it != m_notoSaves.end(); ++it)
		{
			NotoSaves & refNoto = *it;
			if ((++(refNoto.time) > g_Cfg.m_iNotoTimeout) && (g_Cfg.m_iNotoTimeout > 0))
			{
				//m_notoSaves.erase(it);
				NotoSave_Resend(count);
				break;
			}
			count++;
		}
	}
}

void CChar::NotoSave_Resend( int id )
{
	ADDTOCALLSTACK("CChar::NotoSave_Resend()");
	if ( !m_notoSaves.size() )
		return;
	if ( static_cast<int>(m_notoSaves.size()) <= id )
		return;
	NotoSaves & refNotoSave = m_notoSaves.at( id );
	CGrayUID uid = refNotoSave.charUID;
	CChar * pChar = uid.CharFind();
	if ( ! pChar )
		return;
	NotoSave_Delete( pChar, false );
	CObjBase *	pObj	= pChar->GetChar();
	pObj	= dynamic_cast <CObjBase*>( pChar->GetTopLevelObj() );
	if (  GetDist( pObj ) < UO_MAP_VIEW_SIGHT )
		Noto_GetFlag( pChar, true , true );
}

int CChar::NotoSave_GetID( CChar * pChar )
{
	ADDTOCALLSTACK("CChar::NotoSave_GetID(CChar)");
	if ( !pChar )
		return -1;
	if ( !m_notoSaves.size() )
		return -1;
	int count = 0;
	if ( NotoSave() )
	{
		for ( std::vector<NotoSaves>::iterator it = m_notoSaves.begin(); it != m_notoSaves.end(); it++)
		{
			NotoSaves & refNotoSave = m_notoSaves.at(count);
			CGrayUID uid = refNotoSave.charUID;
			if ( uid.CharFind() && uid == static_cast<DWORD>(pChar->GetUID()) )
				return count;
			count++;
		}
	}
	return -1;
}

int CChar::NotoSave_GetID( CGrayUID pChar )
{
	ADDTOCALLSTACK("CChar::NotoSave_GetID(CGrayUID)");
	if ( !m_notoSaves.size() )
		return -1;
	int count = 0;
	if ( NotoSave() )
	{
		for ( std::vector<NotoSaves>::iterator it = m_notoSaves.begin(); it != m_notoSaves.end(); it++)
		{
			NotoSaves & refNotoSave = m_notoSaves.at(count);
			CGrayUID uid = refNotoSave.charUID;
			if ( uid.CharFind() && uid == pChar )
			{
				return count;
			}
			count++;
		}
	}
	return -1;
}

CChar * CChar::NotoSave_GetUID( int index )
{
	ADDTOCALLSTACK("CChar::NotoSave_GetUID");
	if ( !m_notoSaves.size() )
		return NULL;
	if ( static_cast<int>(m_notoSaves.size()) <= index )
		return NULL;
	NotoSaves & refNotoSave = m_notoSaves.at(index);
	CChar * pChar = static_cast<CChar*>( static_cast<CGrayUID>( refNotoSave.charUID ).CharFind() );
	return pChar;
}

bool CChar::NotoSave_Delete( CChar * pChar, bool bForced )
{		
	ADDTOCALLSTACK("CChar::NotoSave_Delete");
	if ( ! pChar )
		return false;
	if ( NotoSave() )
	{
		int count = 0;
		for ( std::vector<NotoSaves>::iterator it = m_notoSaves.begin(); it != m_notoSaves.end(); it++)
		{
			NotoSaves & refNotoSave = m_notoSaves.at(count);
			CGrayUID uid = refNotoSave.charUID;
			if ( uid.CharFind() && uid == static_cast<DWORD>(pChar->GetUID()) )
			{
				m_notoSaves.erase(it);
				return true;
			}
			count++;
		}
	}
	return false;
}
//***************************************************************
// Memory this char has about something in the world.

bool CChar::Memory_UpdateFlags( CItemMemory * pMemory )
{
	ADDTOCALLSTACK("CChar::Memory_UpdateFlags");
	// Reset the check timer based on the type of memory.

	ASSERT(pMemory);
	ASSERT(pMemory->IsType(IT_EQ_MEMORY_OBJ));

	WORD wMemTypes = pMemory->GetMemoryTypes();

	if ( ! wMemTypes )	// No memories here anymore so kill it.
	{
		return false;
	}

	INT64 iCheckTime;
	if ( wMemTypes & MEMORY_ISPAWNED )
	{
		StatFlag_Set( STATF_Spawned );
	}
	else if ( wMemTypes & MEMORY_IPET )
	{
		StatFlag_Set( STATF_Pet );
	}

	if ( wMemTypes & MEMORY_FIGHT )	// update more often to check for retreat.
		iCheckTime = 30*TICK_PER_SEC;
	else if ( wMemTypes & ( MEMORY_IPET | MEMORY_GUARD | MEMORY_ISPAWNED | MEMORY_GUILD | MEMORY_TOWN ))
		iCheckTime = -1;	// never go away.
	else if ( m_pNPC )	// MEMORY_SPEAK
		iCheckTime = 5*60*TICK_PER_SEC;
	else
	{
		iCheckTime = 20*60*TICK_PER_SEC;
	}
	pMemory->SetTimeout( iCheckTime );	// update it's decay time.
	return( true );
}

bool CChar::Memory_UpdateClearTypes( CItemMemory * pMemory, WORD MemTypes )
{
	ADDTOCALLSTACK("CChar::Memory_UpdateClearTypes");
	// Just clear these flags but do not delete the memory.
	// RETURN: true = still useful memory.
	ASSERT(pMemory);

	WORD wPrvMemTypes = pMemory->GetMemoryTypes();
	bool fMore = ( pMemory->SetMemoryTypes( wPrvMemTypes &~ MemTypes ) != 0);

	MemTypes &= wPrvMemTypes;	// Which actually got turned off ?

	if ( MemTypes & MEMORY_ISPAWNED )
	{
		StatFlag_Clear( STATF_Spawned );
		// I am a memory link to another object.
		CItem * pSpawn = pMemory->m_uidLink.ItemFind();
		if ( pSpawn != NULL &&
			pSpawn->IsType(IT_SPAWN_CHAR) &&
			pSpawn->m_itSpawnChar.m_current )
		{
			pSpawn->m_itSpawnChar.m_current --;
		}
	}
	if ( MemTypes & MEMORY_IPET )
	{
		// Am i still a pet of some sort ?
		if ( Memory_FindTypes( MEMORY_IPET ) == NULL )
		{
			StatFlag_Clear( STATF_Pet );
		}
	}

	return fMore && Memory_UpdateFlags( pMemory );
}

void CChar::Memory_AddTypes( CItemMemory * pMemory, WORD MemTypes )
{
	ADDTOCALLSTACK("CChar::Memory_AddTypes");
	if ( pMemory )
	{
		pMemory->SetMemoryTypes( pMemory->GetMemoryTypes() | MemTypes );
		pMemory->m_itEqMemory.m_pt = GetTopPoint();	// Where did the fight start ?
		pMemory->SetTimeStamp(CServTime::GetCurrentTime().GetTimeRaw());
		Memory_UpdateFlags( pMemory );
	}
}

bool CChar::Memory_ClearTypes( CItemMemory * pMemory, WORD MemTypes )
{
	ADDTOCALLSTACK("CChar::Memory_ClearTypes");
	// Clear this memory object of this type.
	if ( pMemory )
	{
		if ( Memory_UpdateClearTypes( pMemory, MemTypes ))
			return true;
		pMemory->Delete();
	}
	return false;
}

CItemMemory * CChar::Memory_CreateObj( CGrayUID uid, WORD MemTypes )
{
	ADDTOCALLSTACK("CChar::Memory_CreateObj");
	// Create a memory about this object.
	// NOTE: Does not check if object already has a memory.!!!
	//  Assume it does not !

	if (( MemTypes & MEMORY_IPET ) && uid.IsItem())
	{
		MemTypes = MEMORY_ISPAWNED;
	}

	CItemMemory * pMemory = dynamic_cast <CItemMemory *>(CItem::CreateBase( ITEMID_MEMORY ));
	if ( pMemory == NULL )
		return( NULL );

	pMemory->SetType(IT_EQ_MEMORY_OBJ);
	pMemory->SetAttr(ATTR_NEWBIE);
	pMemory->m_uidLink = uid;

	Memory_AddTypes( pMemory, MemTypes );
	LayerAdd( pMemory, LAYER_SPECIAL );
	return( pMemory );
}

void CChar::Memory_ClearTypes( WORD MemTypes )
{
	ADDTOCALLSTACK("CChar::Memory_ClearTypes");
	// Remove all the memories of this type.
	CItem* pItemNext;
	CItem* pItem=GetContentHead();
	for ( ; pItem!=NULL; pItem=pItemNext)
	{
		pItemNext = pItem->GetNext();
		if ( ! pItem->IsMemoryTypes(MemTypes))
			continue;
		CItemMemory * pMemory = dynamic_cast <CItemMemory *>(pItem);
		if ( pMemory == NULL )
			continue;
		Memory_ClearTypes( pMemory, MemTypes );
	}
}

CItemMemory * CChar::Memory_FindObj( CGrayUID uid ) const
{
	ADDTOCALLSTACK("CChar::Memory_FindObj");
	// Do I have a memory / link for this object ?
	CItem* pItem=GetContentHead();
	for ( ; pItem!=NULL; pItem=pItem->GetNext())
	{
		if ( ! pItem->IsType(IT_EQ_MEMORY_OBJ))
			continue;
		if ( pItem->m_uidLink != uid )
			continue;
		return( dynamic_cast <CItemMemory *>( pItem ));
	}
	return( NULL );
}

CItemMemory * CChar::Memory_FindTypes( WORD MemTypes ) const
{
	ADDTOCALLSTACK("CChar::Memory_FindTypes");
	// Do we have a certain type of memory.
	// Just find the first one.
	if ( ! MemTypes )
		return( NULL );
	CItem* pItem=GetContentHead();
	for ( ; pItem!=NULL; pItem=pItem->GetNext())
	{
		if ( ! pItem->IsMemoryTypes(MemTypes))
			continue;
		return( dynamic_cast <CItemMemory *>( pItem ));
	}
	return( NULL );
}

TRIGRET_TYPE CChar::OnCharTrigForMemTypeLoop( CScript &s, CTextConsole * pSrc, CScriptTriggerArgs * pArgs, CGString * pResult, WORD wMemType )
{
	ADDTOCALLSTACK("CChar::OnCharTrigForMemTypeLoop");
	CScriptLineContext StartContext = s.GetContext();
	CScriptLineContext EndContext = StartContext;

	if ( wMemType )
	{
		CItem * pItem = GetContentHead();
		CItem * pItemNext;

		for ( ; pItem!=NULL; pItem=pItemNext)
		{
			pItemNext = pItem->GetNext();

			if ( ! pItem->IsMemoryTypes(wMemType))
				continue;
			TRIGRET_TYPE iRet = pItem->OnTriggerRun( s, TRIGRUN_SECTION_TRUE, pSrc, pArgs, pResult );
			if ( iRet == TRIGRET_BREAK )
			{
				EndContext = StartContext;
				break;
			}
			if (( iRet != TRIGRET_ENDIF ) && ( iRet != TRIGRET_CONTINUE ))
				return( iRet );
			if ( iRet == TRIGRET_CONTINUE )
				EndContext = StartContext;
			else
				EndContext = s.GetContext();
			s.SeekContext( StartContext );
		}
	}
	if ( EndContext.m_lOffset <= StartContext.m_lOffset )
	{
		// just skip to the end.
		TRIGRET_TYPE iRet = OnTriggerRun( s, TRIGRUN_SECTION_FALSE, pSrc, pArgs, pResult );
		if ( iRet != TRIGRET_ENDIF )
			return( iRet );
	}
	else
		s.SeekContext( EndContext );
	return( TRIGRET_ENDIF );
}

CItemMemory * CChar::Memory_AddObjTypes( CGrayUID uid, WORD MemTypes )
{
	ADDTOCALLSTACK("CChar::Memory_AddObjTypes");
	CItemMemory * pMemory = Memory_FindObj( uid );
	if ( pMemory == NULL )
	{
		return Memory_CreateObj( uid, MemTypes );
	}
	Memory_AddTypes( pMemory, MemTypes );
	NotoSave_Delete( uid.CharFind(), true );
	return( pMemory );
}

bool CChar::Memory_OnTick( CItemMemory * pMemory )
{
	ADDTOCALLSTACK("CChar::Memory_OnTick");
	// NOTE: Do not return true unless u update the timer !
	// RETURN: false = done with this memory.
	ASSERT(pMemory);

	CObjBase * pObj = pMemory->m_uidLink.ObjFind();
	if ( pObj == NULL )
		return( false );

	if ( pMemory->IsMemoryTypes( MEMORY_FIGHT ))
	{
		// Is the fight still valid ?
		return Memory_Fight_OnTick( pMemory );
	}

	if ( pMemory->IsMemoryTypes( MEMORY_IPET | MEMORY_GUARD | MEMORY_ISPAWNED | MEMORY_GUILD | MEMORY_TOWN ))
		return( true );	// never go away.

	return( false );	// kill it?.
}

//////////////////////////////////////////////////////////////////////////////

void CChar::OnNoticeCrime( CChar * pCriminal, const CChar * pCharMark )
{
	ADDTOCALLSTACK("CChar::OnNoticeCrime");
	// I noticed a crime.
	ASSERT(pCriminal);
	if ( pCriminal == this )
		return;
	
	if ( pCriminal->GetNPCBrain() == NPCBRAIN_GUARD )
		return; 

	if ( pCriminal->IsPriv(PRIV_GM) )
		return;

	// Alert the guards !!!?
	if ( ! NPC_CanSpeak())
		return;	// I can't talk anyhow.

	if ( pCriminal->Noto_Criminal( this ) == true )
		return;

	// NPCBRAIN_BESERK creatures cause criminal fault on the part of their masters.
	if ( pCriminal->m_pNPC && pCriminal->m_pNPC->m_Brain == NPCBRAIN_BERSERK )
	{
		CChar * pOwner = pCriminal->NPC_PetGetOwner();
		if ( pOwner != NULL && pOwner != this )
		{
			OnNoticeCrime( pOwner, pCharMark );
		}
	}

	if ( m_pPlayer )
	{
		// I have the option of attacking the criminal. or calling the guards.
		Memory_AddObjTypes( pCriminal, MEMORY_SAWCRIME );
		return;
	}

	// NPC's can take other actions.

	ASSERT(m_pNPC);
	bool fMyMaster = NPC_IsOwnedBy( pCriminal );

	if ( this != pCharMark )	// it's not me.
	{
		// Thieves and beggars don't care.
		if ( m_pNPC->m_Brain == NPCBRAIN_THIEF || m_pNPC->m_Brain == NPCBRAIN_BEGGAR )
			return;
		if ( fMyMaster )	// I won't rat you out.
			return;
		// Or if the target is evil ?

		// Or if I am evil.
	}
	else
	{
		// I being the victim can retaliate.
		Memory_AddObjTypes( pCriminal, MEMORY_SAWCRIME );
		OnHarmedBy( pCriminal, 1 );
	}

	if ( GetNPCBrain() != NPCBRAIN_HUMAN )
	{
		// Good monsters don't call for guards outside guarded areas.
		if ( ! m_pArea || ! m_pArea->IsGuarded())
			return;
	}

	if ( m_pNPC->m_Brain != NPCBRAIN_GUARD )
	{
		Speak( g_Cfg.GetDefaultMsg( DEFMSG_NPC_GENERIC_CRIM ) );
	}

	// Find a guard.
	CallGuards( pCriminal );
}

bool CChar::CheckCrimeSeen( SKILL_TYPE SkillToSee, CChar * pCharMark, const CObjBase * pItem, LPCTSTR pAction )
{
	ADDTOCALLSTACK("CChar::CheckCrimeSeen");
	// I am commiting a crime.
	// Did others see me commit or try to commit the crime.
	//  SkillToSee = NONE = everyone can notice this.
	// RETURN:
	//  true = somebody saw me.

	bool fSeen = false;

	// Who notices ?
	CWorldSearch AreaChars( GetTopPoint(), UO_MAP_VIEW_SIGHT );
	for (;;)
	{
		CChar * pChar = AreaChars.GetChar();
		if ( pChar == NULL )
			break;
		if ( this == pChar )
			continue;	// I saw myself before.
		if ( ! pChar->CanSeeLOS( this, LOS_NB_WINDOWS )) //what if I was standing behind a window when I saw a crime? :)
			continue;

		bool fYour = ( pCharMark == pChar );

		if ( IsTrigUsed(TRIGGER_SEECRIME) )
		{
			CScriptTriggerArgs Args( pAction );
			Args.m_iN1	= SkillToSee;
			Args.m_iN2	= pItem ? (DWORD) pItem->GetUID() : 0;
			Args.m_pO1	= pCharMark;
			int iRet	= TRIGRET_RET_DEFAULT;

			iRet = pChar->OnTrigger( CTRIG_SeeCrime, this, &Args );

			if ( iRet == TRIGRET_RET_TRUE )
				continue;
			else if ( iRet == TRIGRET_RET_DEFAULT )
			{
				if ( ! g_Cfg.Calc_CrimeSeen( this, pChar, SkillToSee, fYour ))
					continue;
			}
		}

		char *z = Str_GetTemp();
		if ( pAction != NULL )
		{
			if ( pCharMark == NULL )
			{
				sprintf(z, g_Cfg.GetDefaultMsg(DEFMSG_YOUNOTICE_1), GetName(), pAction, pItem->GetName());
			}
			else
			{
				sprintf(z, g_Cfg.GetDefaultMsg(DEFMSG_YOUNOTICE_2),
					static_cast<LPCTSTR>(GetName()), pAction, fYour ? g_Cfg.GetDefaultMsg( DEFMSG_YOUNOTICE_YOUR ) : static_cast<LPCTSTR>(pCharMark->GetName()),
					fYour ? "" : g_Cfg.GetDefaultMsg( DEFMSG_YOUNOTICE_S ),
					static_cast<LPCTSTR>(pItem->GetName()));
			}
			pChar->ObjMessage(z, this);
		}

		// If a GM sees you it it not a crime.
		if ( pChar->GetPrivLevel() > GetPrivLevel())
			continue;
		fSeen = true;

		// They are not a criminal til someone calls the guards !!!
		if ( SkillToSee == SKILL_SNOOPING )
		{
			// Off chance of being a criminal. (hehe)
			if ( ! Calc_GetRandVal( g_Cfg.m_iSnoopCriminal ))
			{
				pChar->OnNoticeCrime( this, pCharMark );
			}
			if ( pChar->m_pNPC )
			{
				pChar->NPC_OnNoticeSnoop( this, pCharMark );
			}
		}
		else
		{
			pChar->OnNoticeCrime( this, pCharMark );
		}
	}
	return( fSeen );
}

bool CChar::Skill_Snoop_Check( const CItemContainer * pItem )
{
	ADDTOCALLSTACK("CChar::Skill_Snoop_Check");
	// Assume the container is not locked.
	// return: true = snoop or can't open at all.
	//  false = instant open.

	if ( pItem == NULL )
		return( true );

	ASSERT( pItem->IsItem());
	if ( pItem->IsContainer() )
	{
		CItemContainer * pItemCont = dynamic_cast <CItemContainer *> (pItem->GetContainer());
			if  ( ( pItemCont->IsItemInTrade() == true )  && ( g_Cfg.m_iTradeWindowSnooping == false ) )
			return( false );
	}

	if ( ! IsPriv(PRIV_GM))
	switch ( pItem->GetType())
	{
		case IT_SHIP_HOLD_LOCK:
		case IT_SHIP_HOLD:
			// Must be on board a ship to open the hatch.
			ASSERT(m_pArea);
			if ( m_pArea->GetResourceID() != pItem->m_uidLink )
			{
				SysMessage(g_Cfg.GetDefaultMsg(DEFMSG_ITEMUSE_HATCH_FAIL));
				return( true );
			}
			break;
		case IT_EQ_BANK_BOX:
			// Some sort of cheater.
			return( false );
		default:
			break;
	}

	CChar * pCharMark;
	if ( ! IsTakeCrime( pItem, &pCharMark ) || pCharMark == NULL )
		return( false );

	if ( Skill_Wait(SKILL_SNOOPING))
		return( true );

	m_Act_Targ = pItem->GetUID();
	Skill_Start( SKILL_SNOOPING );
	return( true );
}

int CChar::Skill_Snooping( SKTRIG_TYPE stage )
{
	ADDTOCALLSTACK("CChar::Skill_Snooping");
	// SKILL_SNOOPING
	// m_Act_Targ = object to snoop into.
	// RETURN:
	// -SKTRIG_QTY = no chance. and not a crime
	// -SKTRIG_FAIL = no chance and caught.
	// 0-100 = difficulty = percent chance of failure.

	if ( stage == SKTRIG_STROKE )
	{
		return 0;
	}

	// Assume the container is not locked.
	CItemContainer * pCont = dynamic_cast <CItemContainer *>(m_Act_Targ.ItemFind());
	if ( pCont == NULL )
	{
		return( -SKTRIG_QTY );
	}

	CChar * pCharMark;
	if ( ! IsTakeCrime( pCont, &pCharMark ) || pCharMark == NULL )
	{
		// Not a crime really.
		return( 0 );
	}

	if ( ! CanTouch( pCont ))
	{
		SysMessageDefault( DEFMSG_SNOOPING_REACH );
		return( -SKTRIG_QTY );
	}

	if ( GetTopDist3D( pCharMark ) > 2 )
	{
		SysMessageDefault( DEFMSG_SNOOPING_MARK );
		return( -SKTRIG_QTY );
	}

	PLEVEL_TYPE plevel = GetPrivLevel();
	bool fForceFail = ( plevel < pCharMark->GetPrivLevel());
	if ( stage == SKTRIG_START )
	{
		if ( fForceFail )
			return( -SKTRIG_FAIL );

		if ( plevel >= PLEVEL_Counsel && plevel > pCharMark->GetPrivLevel())	// i'm higher priv.
			return( 0 );

		// return the difficulty.

		return( pCharMark->Stat_GetAdjusted(STAT_DEX));
	}

	if ( fForceFail )
	{
		stage = SKTRIG_FAIL;
	}

	// did anyone see this ?

	if ( CheckCrimeSeen( SKILL_SNOOPING, pCharMark, pCont, (stage == SKTRIG_FAIL)? g_Cfg.GetDefaultMsg( DEFMSG_SNOOPING_YOUR ) : g_Cfg.GetDefaultMsg( DEFMSG_SNOOPING_SOMEONE ) ))
	{
		Noto_KarmaChangeMessage( -10, -500 );
	}

	//
	// View the container.
	//
	if ( stage == SKTRIG_SUCCESS )
	{
		if ( IsClient())
		{
			m_pClient->addContainerSetup( pCont );
		}
	}
	return( 0 );
}

int CChar::Skill_Stealing( SKTRIG_TYPE stage )
{
	ADDTOCALLSTACK("CChar::Skill_Stealing");
	// m_Act_Targ = object to steal.
	// RETURN:
	// -SKTRIG_QTY = no chance. and not a crime
	// -SKTRIG_FAIL = no chance and caught.
	// 0-100 = difficulty = percent chance of failure.
	//

	if ( stage == SKTRIG_STROKE )
	{
		return 0;
	}

	CItem * pItem = m_Act_Targ.ItemFind();
	CChar * pCharMark = NULL;
	if ( pItem == NULL )	// on a chars head ? = random steal.
	{
		pCharMark = m_Act_Targ.CharFind();
		if ( pCharMark == NULL )
		{
			SysMessageDefault( DEFMSG_STEALING_NOTHING );
			return( -SKTRIG_QTY );
		}
		CItemContainer * pPack = pCharMark->GetPack();
		if ( pPack == NULL )
		{
cantsteal:
			SysMessageDefault( DEFMSG_STEALING_EMPTY );
			return( -SKTRIG_QTY );
		}
		pItem = pPack->ContentFindRandom();
		if ( pItem == NULL )
		{
			goto cantsteal;
		}
		m_Act_Targ = pItem->GetUID();
	}

	// Special cases.
	CContainer * pContainer = dynamic_cast <CContainer *> (pItem->GetContainer());
	if ( pContainer )
	{
		CItemCorpse * pCorpse = dynamic_cast <CItemCorpse *> (pContainer);
		if ( pCorpse )
		{
			SysMessageDefault( DEFMSG_STEALING_CORPSE );
			return( -SKTRIG_ABORT );
		}
	}
   CItem * pCItem = dynamic_cast <CItem *> (pItem->GetContainer());
   if ( pCItem )
   {
	   if ( pCItem->GetType() == IT_GAME_BOARD )
	   {
		   SysMessageDefault( DEFMSG_STEALING_GAMEBOARD );
		   return( -SKTRIG_ABORT );
	   }
	   if ( pCItem->GetType() == IT_EQ_TRADE_WINDOW )
	   {
		   SysMessageDefault( DEFMSG_STEALING_TRADE );
		   return( -SKTRIG_ABORT );
	   }
   }
	CItemCorpse * pCorpse = dynamic_cast <CItemCorpse *> (pItem);
	if ( pCorpse )
	{
		SysMessageDefault( DEFMSG_STEALING_CORPSE );
		return( -SKTRIG_ABORT );
	}
	if ( pItem->IsType(IT_TRAIN_PICKPOCKET))
	{
		SysMessageDefault( DEFMSG_STEALING_PICKPOCKET );
		return -SKTRIG_QTY;
	}
	if ( pItem->IsType( IT_GAME_PIECE ))
	{
		return -SKTRIG_QTY;
	}
	if ( ! CanTouch( pItem ))
	{
		SysMessageDefault( DEFMSG_STEALING_REACH );
		return( -SKTRIG_ABORT );
	}
	if ( ! CanMove( pItem ) ||
		! CanCarry( pItem ))
	{
		SysMessageDefault( DEFMSG_STEALING_HEAVY );
		return( -SKTRIG_ABORT );
	}
	if ( ! IsTakeCrime( pItem, & pCharMark ))
	{
		SysMessageDefault( DEFMSG_STEALING_NONEED );

		// Just pick it up ?

		return( -SKTRIG_QTY );
	}
	if ( m_pArea->IsFlag(REGION_FLAG_SAFE))
	{
		SysMessageDefault( DEFMSG_STEALING_STOP );
		return( -SKTRIG_QTY );
	}

	Reveal();	// If we take an item off the ground we are revealed.

	bool fGround = false;
	if ( pCharMark != NULL )
	{
		if ( GetTopDist3D( pCharMark ) > 2 )
		{
			SysMessageDefault( DEFMSG_STEALING_MARK );
			return -SKTRIG_QTY;
		}
		if ( m_pArea->IsFlag(REGION_FLAG_NO_PVP) && pCharMark->m_pPlayer && ! IsPriv(PRIV_GM))
		{
			SysMessageDefault( DEFMSG_STEALING_SAFE );
			return( -1 );
		}
		if ( GetPrivLevel() < pCharMark->GetPrivLevel())
		{
			return -SKTRIG_FAIL;
		}
		if ( stage == SKTRIG_START )
		{
			return g_Cfg.Calc_StealingItem( this, pItem, pCharMark );
		}
	}
	else
	{
		// stealing off the ground should always succeed.
		// it's just a matter of getting caught.
		if ( stage == SKTRIG_START )
		{
			return 1;	// town stuff on the ground is too easy.
		}
		fGround = true;
	}

	// Deliver the goods.

	if ( stage == SKTRIG_SUCCESS || fGround )
	{
		pItem->ClrAttr(ATTR_OWNED);	// Now it's mine
		CItemContainer * pPack = GetPack();
		if ( pItem->GetParent() != pPack && pPack )
		{
			pItem->RemoveFromView();
			// Put in my invent.
			pPack->ContentAdd( pItem );
		}
	}

	if ( m_Act_Difficulty == 0 )
		return( 0 );	// Too easy to be bad. hehe

	// You should only be able to go down to -1000 karma by stealing.
	if ( CheckCrimeSeen( SKILL_STEALING, pCharMark, pItem, (stage == SKTRIG_FAIL)? g_Cfg.GetDefaultMsg( DEFMSG_STEALING_YOUR ) : g_Cfg.GetDefaultMsg( DEFMSG_STEALING_SOMEONE ) ))
	{
		Noto_KarmaChangeMessage( -100, -1000 );
	}
	return( 0 );
}

void CChar::CallGuards( CChar * pCriminal )
{
	ADDTOCALLSTACK("CChar::CallGuards");
	// I just yelled for guards.
	if ( !m_pArea || ( this == pCriminal ))
		return;

	bool		bSearchedGuard = false;
	CChar		*pGuard = NULL;
	CChar		*pChar;
	bool		bCriminal = false;

	if ( pCriminal && pCriminal->IsPriv(PRIV_GM) )
		return;

	// I'm a guard, why summon someone else to do my work? :)
	if ( !m_pPlayer && m_pNPC->m_Brain == NPCBRAIN_GUARD )
	{
		bSearchedGuard = true;
		pGuard = this;
	}

	if ( !bSearchedGuard )
	{
		// Is there a free guard near by ?
		CWorldSearch AreaGuard(GetTopPoint(), UO_MAP_VIEW_RADAR);
		while ( (pGuard = AreaGuard.GetChar()) != NULL )
		{
			if (( pGuard->m_pPlayer ) || ( pGuard->m_pNPC->m_Brain != NPCBRAIN_GUARD ) || pGuard->IsStatFlag(STATF_War) )
				continue;
			break;
		}
	}

	// Guards can't respond if criminal is outside of the guard zone.
	
	CWorldSearch AreaCrime(GetTopPoint(), UO_MAP_VIEW_SIGHT);
	// Is there anything for guards to see ?
	if ( !pCriminal )
	{
		bSearchedGuard = true;
		while ( (pChar = AreaCrime.GetChar()) != NULL )
		{
			bCriminal = false;	// Asume everyone is inocent.
			if ( pChar == this )
				continue;

			//	scan for guards also ;) it will speed up the execution a bit
			if ( !pGuard && !pChar->m_pPlayer )
			{	
				if ( pChar->GetNPCBrain() == NPCBRAIN_GUARD)
				{
					pGuard = pChar;
					continue;
				}
			}

			// don't allow guards to be called on someone we cant disturb
			if (CanDisturb(pChar) == false)
				continue;

			// mark person a criminal if seen him criming
			// Only players call guards this way. NPC's flag criminal instantly.
			if ( m_pPlayer && Memory_FindObjTypes(pChar, MEMORY_SAWCRIME) )
				pChar->Noto_Criminal();
			
			if ( ( pChar->IsStatFlag( STATF_Criminal ) || ( pChar->Noto_IsEvil() &&  g_Cfg.m_fGuardsOnMurderers) ) && pChar->m_pArea->IsGuarded()  )
				bCriminal = true;

			CVarDefCont * pVarDef = pChar->m_pArea->m_TagDefs.GetKey("OVERRIDE.GUARDS");
			RESOURCE_ID rid = g_Cfg.ResourceGetIDType( RES_CHARDEF, (pVarDef? pVarDef->GetValStr():"GUARDS") );
			if ( IsTrigUsed(TRIGGER_CALLGUARDS) )
			{
				CScriptTriggerArgs args( pGuard );
				args.m_iN1 = rid.GetResIndex();
				args.m_iN2 = 0;
				args.m_iN3 = bCriminal;
				args.m_VarObjs.Insert( 1, pChar, true );

				if ( OnTrigger(CTRIG_CallGuards, pChar, &args) == TRIGRET_RET_TRUE )
					return;

				if ( static_cast<int>(args.m_iN1) != rid.GetResIndex() )
				{
					rid = RESOURCE_ID( RES_CHARDEF, static_cast<int>(args.m_iN1) );
				}
				if ( args.m_iN2 > 0 )
					pGuard = NULL;
					
				if ( args.m_iN3 == 0 )
					bCriminal = false;
				else
					bCriminal = true;
			}
			if ( bCriminal == true )
			{
				if ( !pGuard )			//	spawn a new guard
				{
					if ( !rid.IsValidUID() )
						return;

					pGuard = CChar::CreateNPC(static_cast<CREID_TYPE>(rid.GetResIndex()));
					if ( !pGuard )
						return;

					if ( pGuard->GetNPCBrain() != NPCBRAIN_GUARD)
						pGuard->SetNPCBrain(NPCBRAIN_GUARD);

					//	normal guards, just with patched color hue also acting in red areas
					if ( pChar->m_pArea->m_TagDefs.GetKeyNum("RED", true) )
						pGuard->m_TagDefs.SetNum("NAME.HUE", g_Cfg.m_iColorNotoEvil, true);

					pGuard->Spell_Effect_Create(SPELL_Summon, LAYER_SPELL_Summon, 1000, g_Cfg.m_iGuardLingerTime);
					pGuard->Spell_Teleport(pChar->GetTopPoint(), false, false);
				}
				pGuard->NPC_LookAtCharGuard(pChar, true);
			}
		}
	}
	else
	{
		if ( !pCriminal->m_pArea->IsGuarded() )
			return;
		if ( !bSearchedGuard )
		{
			// Is there a free guard near by ?
			CWorldSearch AreaGuard(GetTopPoint(), UO_MAP_VIEW_RADAR);
			while ( (pGuard = AreaGuard.GetChar()) != NULL )
			{
				if (( pGuard->m_pPlayer ) || ( pGuard->m_pNPC->m_Brain != NPCBRAIN_GUARD ) || pGuard->IsStatFlag(STATF_War) )
					continue;
				break;
			}
		}

		CVarDefCont * pVarDef = pCriminal->m_pArea->m_TagDefs.GetKey("OVERRIDE.GUARDS");
		RESOURCE_ID rid = g_Cfg.ResourceGetIDType( RES_CHARDEF, (pVarDef? pVarDef->GetValStr():"GUARDS") );
		if ( IsTrigUsed(TRIGGER_CALLGUARDS) )
		{
			CScriptTriggerArgs args( pGuard );
			args.m_iN1 = rid.GetResIndex();
			args.m_iN2 = 0;
			args.m_VarObjs.Insert( 1, pCriminal, true );

			if ( OnTrigger(CTRIG_CallGuards, pCriminal, &args) == TRIGRET_RET_TRUE )
				return;

			if ( args.m_iN1 != rid.GetResIndex() )
				rid = RESOURCE_ID( RES_CHARDEF, static_cast<int>(args.m_iN1) );
			if ( args.m_iN2 )
				pGuard = NULL;
		}
		if ( !pGuard )			//	spawn a new guard
		{
			if ( !rid.IsValidUID() )
				return;

			pGuard = CChar::CreateNPC(static_cast<CREID_TYPE>(rid.GetResIndex()));
			if ( !pGuard )
				return;
			
			if ( pGuard->GetNPCBrain() != NPCBRAIN_GUARD)
				pGuard->SetNPCBrain(NPCBRAIN_GUARD);

			//	normal guards, just with patched color hue also acting in red areas
			if ( pCriminal->m_pArea->m_TagDefs.GetKeyNum("RED", true) )
				pGuard->m_TagDefs.SetNum("NAME.HUE", g_Cfg.m_iColorNotoEvil, true);

			pGuard->Spell_Effect_Create(SPELL_Summon, LAYER_SPELL_Summon, 1000, g_Cfg.m_iGuardLingerTime);
			pGuard->Spell_Teleport(pCriminal->GetTopPoint(), false, false);
		}
		pGuard->NPC_LookAtCharGuard(pCriminal, true);
	}
}



void CChar::OnHarmedBy( CChar * pCharSrc, int iHarmQty )
{
	ADDTOCALLSTACK("CChar::OnHarmedBy");
	UNREFERENCED_PARAMETER(iHarmQty);
	// i notice a Crime or attack against me ..
	// Actual harm has taken place.
	// Attack back.

	bool fFightActive = Fight_IsActive();
	Memory_AddObjTypes(pCharSrc, MEMORY_HARMEDBY);

	if ( fFightActive && m_Act_Targ.CharFind() )
	{
		// In war mode already
		if ( m_pPlayer )
			return;
		if ( Calc_GetRandVal( 10 ))
			return;
		// NPC will Change targets.
	}

	if ( NPC_IsOwnedBy(pCharSrc, false) )
	{
		NPC_PetDesert();
	}

	// TORFO patch: war mode to other
	if ( IsClient() )
	{
		StatFlag_Set(STATF_War);
		GetClient()->addPlayerWarMode();
	}

	// I will Auto-Defend myself.
	Fight_Attack(pCharSrc);
	if ( !fFightActive )	// auto defend puts us in war mode.
	{
		UpdateModeFlag();
	}
}

bool CChar::OnAttackedBy( CChar * pCharSrc, int iHarmQty, bool fCommandPet, bool fShouldReveal)
{
	ADDTOCALLSTACK("CChar::OnAttackedBy");
	// We have been attacked in some way by this CChar.
	// Might not actually be doing any real damage. (yet)
	//
	// They may have just commanded their pet to attack me.
	// Cast a bad spell at me.
	// Fired projectile at me.
	// Attempted to provoke me ?
	//
	// RETURN: true = ok.
	//  false = we are immune to this char ! (or they to us)

	if ( pCharSrc == NULL )
		return true;	// field spell ?
	if ( pCharSrc == this )
		return true;	// self induced
	if ( IsStatFlag( STATF_DEAD ) )
		return false;

	if (fShouldReveal)
		pCharSrc->Reveal();	// fix invis exploit

	if ( pCharSrc->IsStatFlag( STATF_INVUL ) && ! pCharSrc->IsPriv( PRIV_GM ))
	{
		// Can't do any damage either.
		pCharSrc->SysMessageDefault( DEFMSG_MAGIC_BLOCK );
		return( false );
	}

	// Am i already attacking the source anyhow
	if ( Fight_IsActive() && m_Act_Targ == pCharSrc->GetUID())
		return true;

	Memory_AddObjTypes( pCharSrc, MEMORY_HARMEDBY|MEMORY_IRRITATEDBY|MEMORY_AGGREIVED );
	Attacker_Add(pCharSrc);

	// Are they a criminal for it ? Is attacking me a crime ?
	if ( Noto_GetFlag(pCharSrc) == NOTO_GOOD )
	{
		if ( IsClient())
		{
			// I decide if this is a crime.
			OnNoticeCrime( pCharSrc, this );
		}
		else
		{
			// If it is a pet then this a crime others can report.
			pCharSrc->CheckCrimeSeen( SKILL_NONE, this, NULL, NULL );
		}
	}

	if ( ! fCommandPet )
	{
		// possibly retaliate. (auto defend)
		OnHarmedBy( pCharSrc, iHarmQty );
	}

	return( true );
}

static const LAYER_TYPE sm_ArmorLayerHead[] = { LAYER_HELM, LAYER_NONE };		// ARMOR_HEAD,
static const LAYER_TYPE sm_ArmorLayerNeck[] = { LAYER_COLLAR, LAYER_NONE };		// ARMOR_NECK,
static const LAYER_TYPE sm_ArmorLayerBack[] = { LAYER_SHIRT, LAYER_CHEST, LAYER_TUNIC, LAYER_CAPE, LAYER_ROBE, LAYER_NONE };	// ARMOR_BACK,
static const LAYER_TYPE sm_ArmorLayerChest[] = { LAYER_SHIRT, LAYER_CHEST, LAYER_TUNIC, LAYER_ROBE, LAYER_NONE };		// ARMOR_CHEST
static const LAYER_TYPE sm_ArmorLayerArms[] = { LAYER_ARMS, LAYER_CAPE, LAYER_ROBE, LAYER_NONE };		// ARMOR_ARMS,
static const LAYER_TYPE sm_ArmorLayerHands[] = { LAYER_GLOVES,	LAYER_NONE };	// ARMOR_HANDS
static const LAYER_TYPE sm_ArmorLayerLegs[] = { LAYER_PANTS, LAYER_SKIRT, LAYER_HALF_APRON, LAYER_ROBE, LAYER_LEGS, LAYER_NONE };	// ARMOR_LEGS,
static const LAYER_TYPE sm_ArmorLayerFeet[] = { LAYER_SHOES, LAYER_LEGS, LAYER_NONE };	// ARMOR_FEET,

struct CArmorLayerType
{
	WORD m_wCoverage;	// Percentage of humanoid body area
	const LAYER_TYPE * m_pLayers;
};

static const CArmorLayerType sm_ArmorLayers[ARMOR_QTY] =	// layers covering the armor zone.
{
	{ 10,	sm_ArmorLayerHead },	// ARMOR_HEAD,
	{ 5,	sm_ArmorLayerNeck },	// ARMOR_NECK,
	{ 10,	sm_ArmorLayerBack },	// ARMOR_BACK,
	{ 30,	sm_ArmorLayerChest },	// ARMOR_CHEST
	{ 10,	sm_ArmorLayerArms },	// ARMOR_ARMS,
	{ 10,	sm_ArmorLayerHands },	// ARMOR_HANDS
	{ 20,	sm_ArmorLayerLegs },	// ARMOR_LEGS,
	{ 5,	sm_ArmorLayerFeet }		// ARMOR_FEET,
};

int CChar::CalcArmorDefense() const
{
	ADDTOCALLSTACK("CChar::CalcArmorDefense");
	// When armor is added or subtracted check this.
	// This is the general AC number printed.
	// Tho not really used to compute damage.

	int iDefenseTotal = 0;
	int iArmorCount = 0;
	WORD ArmorRegionMax[ARMOR_QTY];
#ifndef _WIN32
	for (int i_tmpN=0; i_tmpN < ARMOR_QTY; i_tmpN++)
	{
		ArmorRegionMax[i_tmpN]=0;
	}
#else
	memset( ArmorRegionMax, 0, sizeof(ArmorRegionMax));
#endif
	for ( CItem* pItem=GetContentHead(); pItem!=NULL; pItem=pItem->GetNext())
	{

		CVarDefCont * pVar = pItem->GetDefKey("RESPHYSICAL", true);
		int iDefense = pVar ? static_cast<int>(pVar->GetValNum() + pItem->m_ModAr) : pItem->Armor_GetDefense();

		// IsTypeSpellable() ? ! IT_WAND
		if (( pItem->IsType(IT_SPELL) || pItem->IsTypeArmor()) && pItem->m_itSpell.m_spell )
		{
			SPELL_TYPE spell = static_cast<SPELL_TYPE>(RES_GET_INDEX(pItem->m_itSpell.m_spell));
			switch (spell)
			{
				case SPELL_Steelskin:		// turns your skin into steel, giving a boost to your AR.
				case SPELL_Stoneskin:		// turns your skin into stone, giving a boost to your AR.
				case SPELL_Protection:
				case SPELL_Arch_Prot:
					// Effect of protection spells.
					iDefenseTotal += g_Cfg.GetSpellEffect(spell, pItem->m_itSpell.m_spelllevel ) * 100;
					break;

				default:
					break;
			}
		}
		if ( pVar || iDefense <= 0 )	// If the item has ResPhysical we don't want to check the armor coverage, since the given value is the final value. Also no need to check for items with no armor... less code run.
			continue;

		// reverse of sm_ArmorLayers
		switch ( pItem->GetEquipLayer())
		{
			case LAYER_HELM:		// 6
				if (IsSetCombatFlags(COMBAT_STACKARMOR))
					ArmorRegionMax[ ARMOR_HEAD ] += iDefense;
				else
					ArmorRegionMax[ ARMOR_HEAD ] = maximum( ArmorRegionMax[ ARMOR_HEAD ], iDefense );

				break;
			case LAYER_COLLAR:	// 10 = gorget or necklace.
				if (IsSetCombatFlags(COMBAT_STACKARMOR))
					ArmorRegionMax[ ARMOR_NECK ] += iDefense;
				else
					ArmorRegionMax[ ARMOR_NECK ] = maximum( ArmorRegionMax[ ARMOR_NECK ], iDefense );
				break;
			case LAYER_SHIRT:
			case LAYER_CHEST:	// 13 = armor chest
			case LAYER_TUNIC:	// 17 = jester suit
				if (IsSetCombatFlags(COMBAT_STACKARMOR)) {
					ArmorRegionMax[ ARMOR_CHEST ] += iDefense;
					ArmorRegionMax[ ARMOR_BACK ] += iDefense;
				} else {
					ArmorRegionMax[ ARMOR_CHEST ] = maximum( ArmorRegionMax[ ARMOR_CHEST ], iDefense );
					ArmorRegionMax[ ARMOR_BACK ] = maximum( ArmorRegionMax[ ARMOR_BACK ], iDefense );
				}
				break;
			case LAYER_ARMS:		// 19 = armor
				if (IsSetCombatFlags(COMBAT_STACKARMOR)) 
					ArmorRegionMax[ ARMOR_ARMS ] += iDefense;
				else
					ArmorRegionMax[ ARMOR_ARMS ] = maximum( ArmorRegionMax[ ARMOR_ARMS ], iDefense );
				break;
			case LAYER_PANTS:
			case LAYER_SKIRT:
			case LAYER_HALF_APRON:
				if (IsSetCombatFlags(COMBAT_STACKARMOR)) 
					ArmorRegionMax[ ARMOR_LEGS ] += iDefense;
				else
					ArmorRegionMax[ ARMOR_LEGS ] = maximum( ArmorRegionMax[ ARMOR_LEGS ], iDefense );
				break;
			case LAYER_SHOES:
				if (IsSetCombatFlags(COMBAT_STACKARMOR)) 
					ArmorRegionMax[ ARMOR_FEET ] += iDefense;
				else
					ArmorRegionMax[ ARMOR_FEET ] = maximum( ArmorRegionMax[ ARMOR_FEET ], iDefense );
				break;
			case LAYER_GLOVES:	// 7
				if (IsSetCombatFlags(COMBAT_STACKARMOR)) 
					ArmorRegionMax[ ARMOR_HANDS ] += iDefense;
				else
					ArmorRegionMax[ ARMOR_HANDS ] = maximum( ArmorRegionMax[ ARMOR_HANDS ], iDefense );
				break;
			case LAYER_CAPE:		// 20 = cape
				if (IsSetCombatFlags(COMBAT_STACKARMOR)) {
					ArmorRegionMax[ ARMOR_BACK ] += iDefense;
					ArmorRegionMax[ ARMOR_ARMS ] += iDefense;
				} else {
					ArmorRegionMax[ ARMOR_BACK ] = maximum( ArmorRegionMax[ ARMOR_BACK ], iDefense );
					ArmorRegionMax[ ARMOR_ARMS ] = maximum( ArmorRegionMax[ ARMOR_ARMS ], iDefense );
				}
				break;
			case LAYER_ROBE:		// 22 = robe over all.
				if (IsSetCombatFlags(COMBAT_STACKARMOR)) {
					ArmorRegionMax[ ARMOR_CHEST ] += iDefense;
					ArmorRegionMax[ ARMOR_BACK ] += iDefense;
					ArmorRegionMax[ ARMOR_ARMS ] += iDefense;
					ArmorRegionMax[ ARMOR_LEGS ] += iDefense;
				} else {
					ArmorRegionMax[ ARMOR_CHEST ] = maximum( ArmorRegionMax[ ARMOR_CHEST ], iDefense );
					ArmorRegionMax[ ARMOR_BACK ] = maximum( ArmorRegionMax[ ARMOR_BACK ], iDefense );
					ArmorRegionMax[ ARMOR_ARMS ] = maximum( ArmorRegionMax[ ARMOR_ARMS ], iDefense );
					ArmorRegionMax[ ARMOR_LEGS ] = maximum( ArmorRegionMax[ ARMOR_LEGS ], iDefense );
				}
				break;
			case LAYER_LEGS:
				if (IsSetCombatFlags(COMBAT_STACKARMOR)) {
					ArmorRegionMax[ ARMOR_LEGS ] += iDefense;
					ArmorRegionMax[ ARMOR_FEET ] += iDefense;
				} else {
					ArmorRegionMax[ ARMOR_LEGS ] = maximum( ArmorRegionMax[ ARMOR_LEGS ], iDefense );
					ArmorRegionMax[ ARMOR_FEET ] = maximum( ArmorRegionMax[ ARMOR_FEET ], iDefense );
				}
				break;
			case LAYER_HAND2:
				// Shield effect.
				if ( pItem->IsType( IT_SHIELD ))
				{
					iDefenseTotal += (iDefense) * ( Skill_GetAdjusted(SKILL_PARRYING) / 10 );
				}
				continue;
			default:
				continue;
		}

		iArmorCount ++;
	}
	
	if ( iArmorCount )
	{
		for ( int i=0; i<ARMOR_QTY; i++ )
		{
			iDefenseTotal += sm_ArmorLayers[i].m_wCoverage * ArmorRegionMax[i];
		}
	}

	CVarDefCont * pVar = GetDefKey("RESPHYSICAL", true);
	int ResPhysical = pVar ? static_cast<int>(pVar->GetValNum()) : 0;
	return maximum(( iDefenseTotal / 100 ) + m_ModAr + ResPhysical, 0);
}

int CChar::OnTakeDamage( int iDmg, CChar * pSrc, DAMAGE_TYPE uType )
{
	ADDTOCALLSTACK("CChar::OnTakeDamage");
	// Someone hit us.
	// uType: damage flags
	//  DAMAGE_GOD
	//  DAMAGE_HIT_BLUNT
	//  DAMAGE_MAGIC
	//  ...
	// RETURN: damage done
	//  -1		= already dead / invalid target.
	//  0		= no damage.
	//  INT_MAX	= killed.

	if ( pSrc == NULL )
		pSrc = this;

	short int i_coldDamage = 0;
	short int i_energyDamage = 0;
	short int i_fireDamage = 0;
	short int i_poisonDamage = 0;
	int i_damTemp = 0;
	DAMAGE_TYPE u_damFlag = 0;

	short int i_tDamCount = 1;
	short int i_tDamPois = 0;
	short int i_tDamElec = 0;
	short int i_tDamCold = 0;
	short int i_tDamFire = 0;

	CItem * pActWeapon = pSrc->m_uidWeapon.ItemFind(); 
	CCharBase * pCharDef = Char_GetDef();

	if (!(uType & DAMAGE_FIXED))
	{
		if  ( IsSetCombatFlags(COMBAT_SPECIALDAMAGE) && (uType & (DAMAGE_HIT_BLUNT | DAMAGE_HIT_PIERCE | DAMAGE_HIT_SLASH | DAMAGE_MAGIC)) )
		{
			// special damage can be applied with physical and magical weapons
			CVarDefCont * pValue = pSrc->GetKey("COLDDAMAGE",true);
			if ( pValue ) 
			{
				i_coldDamage = static_cast<short>(pValue->GetValNum());
				uType |= DAMAGE_COLD;
			}
			pValue = pSrc->GetKey("ENERGYDAMAGE",true);
			if ( pValue ) 
			{
				i_energyDamage = static_cast<short>(pValue->GetValNum());
				uType |= DAMAGE_ENERGY;
			}
			pValue = pSrc->GetKey("FIREDAMAGE",true);
			if ( pValue ) 
			{
				i_fireDamage = static_cast<short>(pValue->GetValNum());
				uType |= DAMAGE_FIRE;
			}
			pValue = pSrc->GetKey("POISONDAMAGE",true);
			if ( pValue ) 
			{
				i_poisonDamage = static_cast<short>(pValue->GetValNum());
				uType |= DAMAGE_POISON;
			}
		} 
		else if ( IsSetCombatFlags(COMBAT_USE_RESISTANCE) )
		{
			if ( uType & DAMAGE_HIT_BLUNT )
				i_tDamCount += 1;
			if ( uType & DAMAGE_HIT_PIERCE )
				i_tDamCount += 1;
			if ( uType & DAMAGE_HIT_SLASH )
				i_tDamCount += 1;
			if ( uType & DAMAGE_POISON )
			{
				i_tDamCount += 1;
				i_tDamPois +=1;
			}
			if ( uType & DAMAGE_ENERGY )
			{
				i_tDamCount += 1;
				i_tDamElec +=1;
			}
			if ( uType & DAMAGE_COLD )
			{ 
				i_tDamCount += 1;
				i_tDamCold +=1;
			}
			if ( uType & DAMAGE_FIRE )
			{
				i_tDamCount += 1;
				i_tDamFire +=1;
			}
			if ( uType & ~(DAMAGE_HIT_BLUNT | DAMAGE_HIT_PIERCE | DAMAGE_HIT_SLASH | DAMAGE_POISON | DAMAGE_ENERGY | DAMAGE_COLD | DAMAGE_FIRE) )
				i_tDamCount += 1;
		
			i_tDamPois *= iDmg / i_tDamCount; 
			i_tDamElec *= iDmg / i_tDamCount; 
			i_tDamCold *= iDmg / i_tDamCount; 
			i_tDamFire *= iDmg / i_tDamCount;

			if ( (i_tDamPois + i_tDamElec + i_tDamCold + i_tDamFire) < iDmg )
			{
				iDmg -= (i_tDamPois + i_tDamElec + i_tDamCold + i_tDamFire);
			} 
			else 
			{
				iDmg = 0;
			}

			i_poisonDamage += i_tDamPois;
			i_energyDamage += i_tDamElec;
			i_coldDamage += i_tDamCold;
			i_fireDamage += i_tDamFire;
		}

		i_poisonDamage -= static_cast<short>(i_poisonDamage * GetDefNum("RESPOISON", true)) / 100;
		i_energyDamage -= static_cast<short>(i_energyDamage * GetDefNum("RESENERGY", true)) / 100;
		i_coldDamage -= static_cast<short>(i_coldDamage * GetDefNum("RESCOLD", true)) / 100;
		i_fireDamage -= static_cast<short>(i_fireDamage * GetDefNum("RESFIRE", true)) / 100;

		if ( (iDmg < 0) && 
			 (i_coldDamage < 0) &&
			 (i_energyDamage < 0) &&
			 (i_fireDamage < 0) &&
			 (i_poisonDamage < 0) )	
		{
			//DEBUG_ERR(("resulting Damage == %d, aborting!\n",iDmg));
			return( 0 );
		}

	#ifdef _ALPHASPHERE
		CVarDefCont * pRes = 0;
		int nRes = 0;
		
		if ( uType & DAMAGE_HIT_BLUNT )
		{
			if ( uType & DAMAGE_HIT_SLASH )
			{
				// check for TAG.PHYSRES.SLASH
				pRes = GetKey("PHYSRES.SLASH",true);
				if ( pRes ) 
				{
					nRes = pRes->GetValNum();
					if ( (nRes < -100) || (nRes > 100))
						nRes = 0;
					iDmg += (iDmg * (100 - nRes)) / 100;
				}
			} else if ( uType & DAMAGE_HIT_PIERCE )
			{
				// check for TAG.PHYSRES.PIERCE
				pRes = GetKey("PHYSRES.PIERCE",true);
				if ( pRes ) 
				{
					nRes = pRes->GetValNum();
					if ( (nRes < -100) || (nRes > 100))
						nRes = 0;
					iDmg += (iDmg * (100 - nRes)) / 100;
				}
			} else {
				// check for TAG.PHYSRES.BLUNT
				pRes = GetKey("PHYSRES.BLUNT",true);
				if ( pRes ) 
				{
					nRes = pRes->GetValNum();
					if ( (nRes < -100) || (nRes > 100))
						nRes = 0;
					iDmg += (iDmg * (100 - nRes)) / 100;
				}
			}
		}
	#endif


		//CItem * pActWeapon = pSrc->m_uidWeapon.ItemFind(); 
		if ( (pActWeapon) && IsSetCombatFlags(COMBAT_OSIDAMAGEMOD) && (uType & (DAMAGE_HIT_BLUNT | DAMAGE_HIT_PIERCE | DAMAGE_HIT_SLASH)) )
		{
			int damMod = 0;
			int damModTemp = 0;
			//Lumberjack Bonus
			if ( pActWeapon->IsType(IT_WEAPON_AXE) || pActWeapon->IsType(IT_WEAPON_SWORD) )
			{
				damModTemp = pSrc->Skill_GetBase(SKILL_LUMBERJACKING) / 50;
				if (pSrc->Skill_GetBase(SKILL_LUMBERJACKING) >= 1000)
					damModTemp += 10;
			}
			damMod += damModTemp;

			//Tactics Bonus
			damModTemp = pSrc->Skill_GetBase(SKILL_TACTICS) / 16;
			if (pSrc->Skill_GetBase(SKILL_TACTICS) >= 1000)
				damModTemp += 6;
			damMod += damModTemp;

			//Anatomy Bonus
			damModTemp = pSrc->Skill_GetBase(SKILL_ANATOMY) / 20;
			if (pSrc->Skill_GetBase(SKILL_ANATOMY) >= 1000)
				damModTemp += 5;
			damMod += damModTemp;

			//Strenght Bonus
			damModTemp = pSrc->Stat_GetAdjusted(STAT_STR) / 3;
			if (pSrc->Stat_GetAdjusted(STAT_STR) >= 100)
				damModTemp += 5;
			damMod += damModTemp;

			//No cap for OSI...   100% cap on Damage Increase Items only
			//if ( damMod > 100)
			//	damMod=100;
			iDmg += iDmg * (damMod / 100);
		}

		i_damTemp = iDmg + i_coldDamage + i_energyDamage + i_fireDamage + i_poisonDamage;
		u_damFlag = uType;
	}
	else
	{
		i_damTemp = iDmg;
		u_damFlag = uType;
	}

	if ( IsTrigUsed(TRIGGER_GETHIT) )
	{
		CScriptTriggerArgs Args( i_damTemp, u_damFlag, static_cast<INT64>(0) );
		if ( OnTrigger( CTRIG_GetHit, pSrc, &Args ) == TRIGRET_RET_TRUE )
			return( 0 );
		iDmg = static_cast<int>(Args.m_iN1);
		uType = static_cast<DAMAGE_TYPE>(Args.m_iN2);
	}

	if (!(uType & DAMAGE_FIXED))
	{
		if ( IsSetCombatFlags(COMBAT_USE_RESISTANCE) )
		{
			i_tDamCount = 1;
 			i_tDamPois = 0;
			i_tDamElec = 0;
			i_tDamCold = 0;
			i_tDamFire = 0;

			if ( uType & (DAMAGE_HIT_BLUNT|DAMAGE_HIT_PIERCE|DAMAGE_HIT_SLASH) )
				i_tDamCount += 1;
			if ( uType & DAMAGE_POISON )
			{
				i_tDamCount += 1;
				i_tDamPois +=1;
			}
			if ( uType & DAMAGE_ENERGY )
			{
				i_tDamCount += 1;
				i_tDamElec +=1;
			}
			if ( uType & DAMAGE_COLD )
			{
				i_tDamCount += 1;
				i_tDamCold +=1;
			}
			if ( uType & DAMAGE_FIRE )
			{
				i_tDamCount += 1;
				i_tDamFire +=1;
			}
			if ( uType & ~(DAMAGE_HIT_BLUNT | DAMAGE_HIT_PIERCE | DAMAGE_HIT_SLASH | DAMAGE_POISON | DAMAGE_ENERGY | DAMAGE_COLD | DAMAGE_FIRE) )
			{
				i_tDamCount += 1;
			}
	
			i_tDamPois *= iDmg / i_tDamCount; 
			i_tDamElec *= iDmg / i_tDamCount; 
			i_tDamCold *= iDmg / i_tDamCount; 
			i_tDamFire *= iDmg / i_tDamCount;

			if ( (i_tDamPois + i_tDamElec + i_tDamCold + i_tDamFire) < iDmg )
			{
				iDmg -= (i_tDamPois + i_tDamElec + i_tDamCold + i_tDamFire);
			} 
			else 
			{
				iDmg = 0;
			}

			i_poisonDamage = i_tDamPois;
			i_energyDamage = i_tDamElec;
			i_coldDamage = i_tDamCold;
			i_fireDamage = i_tDamFire;
		}

		i_poisonDamage -= static_cast<short>(i_poisonDamage * GetDefNum("RESPOISON", true)) / 100;
		i_energyDamage -= static_cast<short>(i_energyDamage * GetDefNum("RESENERGY", true)) / 100;
		i_coldDamage -= static_cast<short>(i_coldDamage * GetDefNum("RESCOLD", true)) / 100;
		i_fireDamage -= static_cast<short>(i_fireDamage * GetDefNum("RESFIRE", true)) / 100;

		if ( (iDmg < 0) && 
			 (i_coldDamage < 0) &&
			 (i_energyDamage < 0) &&
			 (i_fireDamage < 0) &&
			 (i_poisonDamage < 0) )	
		{
			//DEBUG_ERR(("resulting Damage == %d, aborting!\n",iDmg));
			return( 0 );
		}
	

		if (( uType & ( DAMAGE_ENERGY | DAMAGE_HIT_BLUNT | DAMAGE_HIT_PIERCE | DAMAGE_HIT_SLASH | DAMAGE_FIRE | DAMAGE_MAGIC )) && (!( uType & DAMAGE_NOUNPARALYZE )))
		{
			if (LayerFind( LAYER_FLAG_Stuck ))
				LayerFind( LAYER_FLAG_Stuck )->Delete();
			StatFlag_Clear( STATF_Freeze );	// remove paralyze.
		}

		//CCharBase * pCharDef = Char_GetDef();
		ASSERT(pCharDef);

		if ( uType & ( DAMAGE_HIT_BLUNT | DAMAGE_HIT_PIERCE | DAMAGE_HIT_SLASH ))
		{
			// A physical blow of some sort.
			// Try to allow the armor or shield to take some damage.
			Reveal();

			// Check for reactive armor.
			if ( IsStatFlag(STATF_Reactive) && ! ( uType & DAMAGE_GOD ) )
			{
				// reflect some damage back.
				if ( !pSrc || pSrc == this ) ;
				else if ( pSrc->m_pNPC && ( pSrc->m_pNPC->m_Brain == NPCBRAIN_GUARD )) ; // guards should not react by reactive armor
				else if ( GetTopDist3D(pSrc) <= 2 )
				{
					// Spell strength is NOT the same as MAGERY !!!???
					int iSkillVal = Skill_GetAdjusted(SKILL_MAGERY);
					int iEffect = g_Cfg.GetSpellEffect( SPELL_Reactive_Armor, iSkillVal );
					int iRefDam = Calc_GetRandVal( IMULDIV( iDmg, iEffect, 1000 ));

					// make sure the reflected damage is between 0 and iDmg-1 or else
					// 2 reactive armour users could get caught in an infinite loop of
					// reflecting damage
					//iRefDam = maximum( minimum( iRefDam, iDmg - 1 ), 0 );
					iRefDam = medium(0, iRefDam, iDmg - 1);

					if ( iRefDam > 0 )
					{
						iDmg -= iRefDam;
						pSrc->OnTakeDamage( iRefDam, this, uType );
						pSrc->Effect( EFFECT_OBJ, ITEMID_FX_CURSE_EFFECT, this, 9, 6 );
					}
				}
			}

			// Armour calculations
			if ( ! ( uType & DAMAGE_GOD) )
			{
				if ( ! ( uType & DAMAGE_GENERAL ))
				{
					// Armour calculation for normal damage (a specific part of the body/armour is hit and damaged)
					int iDef = Calc_GetRandVal( maximum(pCharDef->m_defense + m_ModAr, 0) );
					iDmg = OnTakeDamageHitPoint( iDmg, pSrc, uType );

					if ( uType & DAMAGE_MAGIC )
					{
						// Magical damage halves effectiveness of defense (or bypasses it with MAGICF_IGNOREAR)
						if ( IsSetMagicFlags( MAGICF_IGNOREAR ) )
						{
							iDef = 0;
						} 
						else 
						{
							iDef /= 2;
						}
					}

					iDmg -= iDef;
				}
				else
				{
					// Armour calculation for general damage (not hitting a specific body part or armour piece)
					// MAGICF_IGNOREAR bypasses defense completely
					if ( !IsSetMagicFlags( MAGICF_IGNOREAR ) || !(uType & DAMAGE_MAGIC) )
					{
						iDmg -= Calc_GetRandVal( m_defense + pCharDef->m_defense );
						// ??? take some random damage to my equipped items.
					}
				}
			}
		}


		if ( IsStatFlag( STATF_INVUL ))
		{
	effect_bounce:
			if ( iDmg )
			{
				Effect( EFFECT_OBJ, ITEMID_FX_GLOW, this, 9, 30, false );
			}
			iDmg = 0;
		}
		else if ( ! ( uType & DAMAGE_GOD ))
		{
			if ( m_pArea )
			{
				if ( m_pArea->IsFlag(REGION_FLAG_SAFE))
					goto effect_bounce;
				if ( m_pArea->IsFlag(REGION_FLAG_NO_PVP) && pSrc && (( IsStatFlag(STATF_Pet) &&  NPC_PetGetOwner() == pSrc) || (m_pPlayer && ( pSrc->m_pPlayer || pSrc->IsStatFlag(STATF_Pet)) ) ))
					goto effect_bounce;
			}
			if ( IsStatFlag(STATF_Stone))	// can't hurt us anyhow.
			{
				goto effect_bounce;
			}
		}

		if ( Stat_GetVal(STAT_STR) <= 0 || IsStatFlag(STATF_DEAD) )	// Already dead.
			return( -1 );

		if ( uType & DAMAGE_FIRE )
		{
			if ( pCharDef->Can(CAN_C_FIRE_IMMUNE)) // immune to the fire part.
			{
				// If there is any other sort of damage then dish it out as well.
				if ( ! ( uType & ( DAMAGE_HIT_BLUNT | DAMAGE_HIT_PIERCE | DAMAGE_HIT_SLASH | DAMAGE_POISON | DAMAGE_ENERGY | DAMAGE_COLD)))
					return( 0 );	// No effect.
				iDmg /= 2;
			}
		}

		iDmg += i_coldDamage + i_energyDamage + i_fireDamage + i_poisonDamage;
	}

	// defend myself. (even though it may not have hurt me.)
	// Don't reveal attacker if the damage has DAMAGE_NOREVEAL flag set
	// This is set by default for poison and spell damage.

	if ( ! OnAttackedBy( pSrc, iDmg, false, !(uType & DAMAGE_NOREVEAL) ))
		return( 0 );

	if ( pSrc && pSrc != this ) // record to last attackers
	{
		bool bAttackerExists = false;
		for (std::vector<LastAttackers>::iterator it = m_lastAttackers.begin(); it != m_lastAttackers.end(); ++it)
		{
			LastAttackers & refAttacker = *it;
			if ( refAttacker.charUID == pSrc->GetUID().GetPrivateUID() )
			{
				refAttacker.elapsed = 0;
				refAttacker.amountDone += maximum( 0, iDmg );
				refAttacker.threat += maximum( 0, iDmg);
				bAttackerExists = true;
				break;
			}
		}

		// Attacker not found
		if (bAttackerExists == false)
		{
			LastAttackers attacker;
			attacker.amountDone = maximum( 0, iDmg );
			attacker.charUID = pSrc->GetUID().GetPrivateUID();
			attacker.threat = maximum( 0, iDmg);
			attacker.elapsed = 0;
			m_lastAttackers.push_back(attacker);
		}
	}

	// Did it hurt ?
	if ( iDmg <= 0 )
		return( 0 );

	// Make blood depending on hit damage. assuming the creature has blood
	if ( m_wBloodHue != static_cast<HUE_TYPE>(-1) )
	{
		ITEMID_TYPE id = ITEMID_NOTHING;
		if ( iDmg > 10 )
		{
			id = static_cast<ITEMID_TYPE>(ITEMID_BLOOD1 + Calc_GetRandVal(ITEMID_BLOOD6-ITEMID_BLOOD1));
		}
		else if ( Calc_GetRandVal( iDmg ) > 5 )
		{
			id = ITEMID_BLOOD_SPLAT;	// minor hit.
		}
		if ( id &&
			! IsStatFlag( STATF_Conjured ) &&
			( uType & ( DAMAGE_HIT_PIERCE | DAMAGE_HIT_SLASH )))	// A blow of some sort.
		{
			CItem * pBlood = CItem::CreateBase( id );
			ASSERT(pBlood);
			pBlood->SetHue( m_wBloodHue );
			pBlood->MoveToDecay( GetTopPoint(), 7*TICK_PER_SEC );
		}
	}

	UpdateStatVal( STAT_STR, -iDmg );
	// always send updates to src
	if ( pSrc->IsClient() )
		pSrc->GetClient()->addHitsUpdate( GetUID() );

	if ( IsAosFlagEnabled( FEATURE_AOS_DAMAGE ) )
	{
		if ( IsClient() )
			m_pClient->addShowDamage( iDmg, (DWORD)GetUID() );
		if ( pSrc->IsClient() && (GetUID() != pSrc->GetUID()) )
			pSrc->m_pClient->addShowDamage( iDmg, (DWORD)GetUID() );
		else
		{
			CChar * pSrcOwner = pSrc->NPC_PetGetOwner();
			if ( pSrcOwner != NULL )
			{
				if ( pSrcOwner->IsClient() )
					pSrcOwner->m_pClient->addShowDamage( iDmg, (DWORD)GetUID() );
			}
		}
	}

	if ( Stat_GetVal(STAT_STR) <= 0 )
	{
		// We will die from this...make sure the killer is set correctly...if we don't do this, the person we are currently
		// attacking will get credit for killing us.
		// Killed by a guard looks here !
		m_Act_Targ = pSrc->GetUID();
		if (IsStatFlag( STATF_Ridden )) // Dead Horse?
		{
			CChar *pCharRider = Horse_GetMountChar();
			if ( pCharRider )
				pCharRider->Horse_UnMount();
		}
		return( -1 );	// INT_MAX ?
	}

	SoundChar( CRESND_GETHIT );
	if ( m_atFight.m_War_Swing_State != WAR_SWING_SWINGING )	// Not interrupt my swing.
	{
		UpdateAnimate( ANIM_GET_HIT );
	}
	return( iDmg );
}

int CChar::OnTakeDamageHitPoint( int iDmg, CChar * pSrc, DAMAGE_TYPE uType )
{
	ADDTOCALLSTACK("CChar::OnTakeDamageHitPoint");
	// ( DAMAGE_HIT_BLUNT | DAMAGE_HIT_PIERCE | DAMAGE_HIT_SLASH )
	// Point strike type damage.
	// Deflect some damage with shield or weapon ?

	ASSERT( ! ( uType & DAMAGE_GENERAL ));

	if ( IsStatFlag(STATF_HasShield) &&
		(uType & ( DAMAGE_HIT_BLUNT | DAMAGE_HIT_PIERCE | DAMAGE_HIT_SLASH )) &&
		! (uType & (DAMAGE_GOD|DAMAGE_ENERGY)))
	{
		CItem * pShield = LayerFind( LAYER_HAND2 );
		if ( pShield != NULL && Skill_UseQuick( SKILL_PARRYING, Calc_GetRandLLVal((pSrc!=NULL) ? (pSrc->Skill_GetBase(SKILL_TACTICS)/10) : 100 )))
		{
			// Damage the shield.
			// Let through some damage.
			int iDefense = Calc_GetRandVal( pShield->Armor_GetDefense() / 2 );
			if ( pShield->OnTakeDamage( minimum( iDmg, iDefense ), pSrc, uType ))
			{
				SysMessageDefault( DEFMSG_COMBAT_PARRY );
			}
			iDmg -= iDefense; // damage absorbed by shield
		}
	}

	// Assumes humanoid type body. Orcs, Headless, trolls, humans etc.
	// ??? If not humanoid ??
	CCharBase * pCharDef = Char_GetDef();
	ASSERT(pCharDef);

	if ( pCharDef->Can( CAN_C_NONHUMANOID ))
	{
		// ??? we may want some sort of message ?
		return( iDmg );
	}

	// Where was the hit ?
	int iHitRoll = Calc_GetRandVal( 100 ); // determine area of body hit
	BODYPART_TYPE iHitArea=ARMOR_HEAD;

	int iHitPref = 0;
	int iHitPrefChance = pSrc->Skill_GetBase(SKILL_TACTICS);
	int iHitPrefPenalty = 30;	// means: percent
	int iHitPrefBonus = 30;		// means: percent

	CVarDefCont * pHitPref = pSrc->GetKey("HITPREFERENCE", true); 
	if ( pHitPref )
		iHitPref = static_cast<int>(pHitPref->GetValNum());	// 1 .. 8
	CVarDefCont * pHitPrefChance = pSrc->GetKey("HITPREFERENCE_CHANCE", true); 
	if ( pHitPrefChance )
		iHitPrefChance = static_cast<int>(pHitPrefChance->GetValNum());	// 1 .. 8
	CVarDefCont * pHitPrefPenalty = pSrc->GetKey("HITPREFERENCE_PENALTY", true); 
	if ( pHitPrefPenalty )
		iHitPrefPenalty = static_cast<int>(pHitPrefPenalty->GetValNum());	// 1 .. 8
	CVarDefCont * pHitPrefBonus = pSrc->GetKey("HITPREFERENCE_BONUS", true); 
	if ( pHitPrefBonus )
		iHitPrefBonus = static_cast<int>(pHitPrefBonus->GetValNum());	// 1 .. 8

	if ( IsSetCombatFlags(COMBAT_TARGETTEDHIT) && 
		 (iHitPref != 0) && 
		 (uType & ( DAMAGE_HIT_BLUNT | DAMAGE_HIT_PIERCE | DAMAGE_HIT_SLASH )) && 
		 ! (uType & (DAMAGE_GENERAL|DAMAGE_GOD)) ) {
		if ( Calc_GetRandVal( 1100 ) < iHitPrefChance ) {
			while (iHitPref > ARMOR_QTY)	// gt8 - 8 to get a valid bodypart number +1
				iHitPref -= ARMOR_QTY;
			iHitArea = static_cast<BODYPART_TYPE>(iHitPref - 1);	// 1->0, 2->1, ... 8->7
			iDmg += (iDmg * iHitPrefBonus) / 100;
		} else {
			iDmg -= (iDmg * iHitPrefPenalty) / 100;
			pSrc->SysMessage(g_Cfg.GetDefaultMsg( DEFMSG_COMBAT_TARGET_MISSED ));
			while ( iHitArea<ARMOR_QTY-1 )
			{
				iHitRoll -= sm_ArmorLayers[iHitArea].m_wCoverage;
				if ( iHitRoll < 0 )
					break;
				iHitArea = static_cast<BODYPART_TYPE>( iHitArea + 1 );
			}
		}
	} else {
		while ( iHitArea < (ARMOR_QTY - 1) )
		{
			iHitRoll -= sm_ArmorLayers[iHitArea].m_wCoverage;
			if ( iHitRoll < 0 )
				break;
			iHitArea = static_cast<BODYPART_TYPE>( iHitArea + 1 );
		}
	}

//	while ( iHitArea<ARMOR_QTY-1 )
//	{
//		iHitRoll -= sm_ArmorLayers[iHitArea].m_wCoverage;
//		if ( iHitRoll < 0 )
//			break;
//		iHitArea = (BODYPART_TYPE)( iHitArea + 1 );
//	}

	if ( (uType & ( DAMAGE_HIT_BLUNT | DAMAGE_HIT_PIERCE | DAMAGE_HIT_SLASH |DAMAGE_FIRE|DAMAGE_ENERGY)) &&
		! (uType & (DAMAGE_GENERAL|DAMAGE_GOD)))
	{

		static LPCTSTR const sm_Hit_Head1[][2] =
		{
			{ g_Cfg.GetDefaultMsg( DEFMSG_COMBAT_HIT_HEAD1S ), g_Cfg.GetDefaultMsg( DEFMSG_COMBAT_HIT_HEAD1O ) },
			{ g_Cfg.GetDefaultMsg( DEFMSG_COMBAT_HIT_HEAD2S ), g_Cfg.GetDefaultMsg( DEFMSG_COMBAT_HIT_HEAD2O ) },
			{ g_Cfg.GetDefaultMsg( DEFMSG_COMBAT_HIT_HEAD3S ), g_Cfg.GetDefaultMsg( DEFMSG_COMBAT_HIT_HEAD3O ) }
		};
		static LPCTSTR const sm_Hit_Head2[][2] =
		{
			{ g_Cfg.GetDefaultMsg( DEFMSG_COMBAT_HIT_HEAD4S ), g_Cfg.GetDefaultMsg( DEFMSG_COMBAT_HIT_HEAD4O ) },
			{ g_Cfg.GetDefaultMsg( DEFMSG_COMBAT_HIT_HEAD5S ), g_Cfg.GetDefaultMsg( DEFMSG_COMBAT_HIT_HEAD5O ) },
			{ g_Cfg.GetDefaultMsg( DEFMSG_COMBAT_HIT_HEAD6S ), g_Cfg.GetDefaultMsg( DEFMSG_COMBAT_HIT_HEAD6O ) }
		};
		static LPCTSTR const sm_Hit_Chest1[][2] =
		{
			{ g_Cfg.GetDefaultMsg( DEFMSG_COMBAT_HIT_CHEST1S ), g_Cfg.GetDefaultMsg( DEFMSG_COMBAT_HIT_CHEST1O ) },
			{ g_Cfg.GetDefaultMsg( DEFMSG_COMBAT_HIT_CHEST2S ), g_Cfg.GetDefaultMsg( DEFMSG_COMBAT_HIT_CHEST2O ) },
			{ g_Cfg.GetDefaultMsg( DEFMSG_COMBAT_HIT_CHEST3S ), g_Cfg.GetDefaultMsg( DEFMSG_COMBAT_HIT_CHEST3O ) }
		};
		static LPCTSTR const sm_Hit_Chest2[][2] =
		{
			{ g_Cfg.GetDefaultMsg( DEFMSG_COMBAT_HIT_CHEST4S ), g_Cfg.GetDefaultMsg( DEFMSG_COMBAT_HIT_CHEST4O ) },
			{ g_Cfg.GetDefaultMsg( DEFMSG_COMBAT_HIT_CHEST5S ), g_Cfg.GetDefaultMsg( DEFMSG_COMBAT_HIT_CHEST5O ) },
			{ g_Cfg.GetDefaultMsg( DEFMSG_COMBAT_HIT_CHEST6S ), g_Cfg.GetDefaultMsg( DEFMSG_COMBAT_HIT_CHEST6O ) }
		};
		static LPCTSTR const sm_Hit_Arm[][2] =
		{
			{ g_Cfg.GetDefaultMsg( DEFMSG_COMBAT_HIT_ARM1S ), g_Cfg.GetDefaultMsg( DEFMSG_COMBAT_HIT_ARM1O ) },
			{ g_Cfg.GetDefaultMsg( DEFMSG_COMBAT_HIT_ARM2S ), g_Cfg.GetDefaultMsg( DEFMSG_COMBAT_HIT_ARM2O ) },
			{ g_Cfg.GetDefaultMsg( DEFMSG_COMBAT_HIT_ARM3S ), g_Cfg.GetDefaultMsg( DEFMSG_COMBAT_HIT_ARM3O ) }
		};
		static LPCTSTR const sm_Hit_Legs[][2] =
		{
			{ g_Cfg.GetDefaultMsg( DEFMSG_COMBAT_HIT_LEGS1S ), g_Cfg.GetDefaultMsg( DEFMSG_COMBAT_HIT_LEGS1O ) },
			{ g_Cfg.GetDefaultMsg( DEFMSG_COMBAT_HIT_LEGS2S ), g_Cfg.GetDefaultMsg( DEFMSG_COMBAT_HIT_LEGS2O ) },
			{ g_Cfg.GetDefaultMsg( DEFMSG_COMBAT_HIT_LEGS3S ), g_Cfg.GetDefaultMsg( DEFMSG_COMBAT_HIT_LEGS3O ) }
		};
		static LPCTSTR const sm_Hit_Hands[][2] =	// later include exclusion of left hand if have shield
		{
			{ g_Cfg.GetDefaultMsg( DEFMSG_COMBAT_HIT_HAND1S ), g_Cfg.GetDefaultMsg( DEFMSG_COMBAT_HIT_HAND1O ) },
			{ g_Cfg.GetDefaultMsg( DEFMSG_COMBAT_HIT_HAND2S ), g_Cfg.GetDefaultMsg( DEFMSG_COMBAT_HIT_HAND2O ) },
			{ g_Cfg.GetDefaultMsg( DEFMSG_COMBAT_HIT_HAND3S ), g_Cfg.GetDefaultMsg( DEFMSG_COMBAT_HIT_HAND3O ) }
		};
		static LPCTSTR const sm_Hit_Neck1[2] =
		{
			g_Cfg.GetDefaultMsg( DEFMSG_COMBAT_HIT_NECK1S ), g_Cfg.GetDefaultMsg( DEFMSG_COMBAT_HIT_NECK1O )
		};
		static LPCTSTR const sm_Hit_Neck2[2] =
		{
			g_Cfg.GetDefaultMsg( DEFMSG_COMBAT_HIT_NECK2S ), g_Cfg.GetDefaultMsg( DEFMSG_COMBAT_HIT_NECK2O )
		};
		static LPCTSTR const sm_Hit_Back[2] =
		{
			g_Cfg.GetDefaultMsg( DEFMSG_COMBAT_HIT_BACK1S ), g_Cfg.GetDefaultMsg( DEFMSG_COMBAT_HIT_BACK1O )
		};
		static LPCTSTR const sm_Hit_Feet[2] =
		{
			g_Cfg.GetDefaultMsg( DEFMSG_COMBAT_HIT_FEET1S ), g_Cfg.GetDefaultMsg( DEFMSG_COMBAT_HIT_FEET1O )
		};

		int iMsg = Calc_GetRandVal(3);
		LPCTSTR const * ppMsg;
		switch ( iHitArea )
		{
			case ARMOR_HEAD:
				ppMsg = (iDmg>10) ? sm_Hit_Head2[iMsg] : sm_Hit_Head1[iMsg];
				break;
			case ARMOR_NECK:
				ppMsg = (iDmg>10) ? sm_Hit_Neck2 : sm_Hit_Neck1;
				break;
			case ARMOR_BACK:
				ppMsg = sm_Hit_Back;
				break;
			case ARMOR_CHEST:
				ppMsg = (iDmg>10) ? sm_Hit_Chest2[iMsg] : sm_Hit_Chest1[iMsg];
				break;
			case ARMOR_ARMS:
				ppMsg = sm_Hit_Arm[iMsg];
				break;
			case ARMOR_HANDS:
				ppMsg = sm_Hit_Hands[iMsg];
				break;
			case ARMOR_LEGS:
				ppMsg = sm_Hit_Legs[iMsg];
				break;
			case ARMOR_FEET:
				ppMsg = sm_Hit_Feet;
				break;
			default:
				ASSERT(0);
				ppMsg = NULL;
				break;
		}

		if ( pSrc != this && ppMsg != NULL )
		{
			if ( IsPriv(PRIV_DETAIL))
			{
				SysMessagef( ppMsg[0], ( pSrc == NULL ) ? "It" : static_cast<LPCTSTR>(pSrc->GetName()));
			}
			if ( pSrc != NULL && pSrc->IsPriv(PRIV_DETAIL))
			{
				pSrc->SysMessagef( ppMsg[1], static_cast<LPCTSTR>(GetName()));
			}
		}
	}

	// Do damage to my armor. (what if it is empty ?)

	int iMaxCoverage = 0;	// coverage at the hit zone.

	// iDmg = ( iDmg * GW_GetSCurve( iMaxCoverage - iDmg, 10 )) / 100;
	if ( IsSetMagicFlags( MAGICF_IGNOREAR ) && ( uType & DAMAGE_MAGIC ))		// Moved from bottom to here because we don't need to check the following in this case... performance! Also, if MAGICF_IGNOREAR is set ... it makes no sense to damage the armors
		return iDmg;

	CItem * pArmorNext;
	for ( CItem * pArmor = GetContentHead(); pArmor != NULL; pArmor = pArmorNext )
	{
		pArmorNext = pArmor->GetNext();

		if ( pArmor->IsType( IT_SPELL ) && pArmor->m_itSpell.m_spell )
		{
			SPELL_TYPE spell = static_cast<SPELL_TYPE>(RES_GET_INDEX(pArmor->m_itSpell.m_spell));
			switch ( spell )
			{
				case SPELL_Steelskin:		// turns your skin into steel, giving a boost to your AR.
				case SPELL_Stoneskin:		// turns your skin into stone, giving a boost to your AR.
				case SPELL_Arch_Prot:
				case SPELL_Protection:
					// Effect of protection spells are general.
					iMaxCoverage = maximum( iMaxCoverage, g_Cfg.GetSpellEffect( spell, pArmor->m_itSpell.m_spelllevel ));
					continue;
				default:
					break;
			}
		}

		LAYER_TYPE layer = pArmor->GetEquipLayer();
		if ( ! CItemBase::IsVisibleLayer( layer ))
			continue;

		const CArmorLayerType * pArmorLayer = &sm_ArmorLayers[iHitArea];

		for ( size_t i = 0; pArmorLayer->m_pLayers[i] != LAYER_NONE; i++ ) // layers covering the armor zone.
		{
			if ( pArmorLayer->m_pLayers[i] == layer )
			{
				// This piece of armor takes damage.

				if (IsSetCombatFlags(COMBAT_STACKARMOR))
					iMaxCoverage += pArmor->Armor_GetDefense();
				else
					iMaxCoverage = maximum( iMaxCoverage, pArmor->Armor_GetDefense());

				pArmor->OnTakeDamage( iDmg, pSrc, uType );
				break;
			}
		}
	}

	iDmg -= Calc_GetRandVal( iMaxCoverage );
	return( iDmg );
}

//*******************************************************************************
// Fight specific memories.

void CChar::Memory_Fight_Retreat( CChar * pTarg, CItemMemory * pFight )
{
	ADDTOCALLSTACK("CChar::Memory_Fight_Retreat");
	// The fight is over because somebody ran away.
	if ( pTarg == NULL || pTarg->IsStatFlag( STATF_DEAD ))
		return;

	ASSERT(pFight);
	int iMyDistFromBattle = GetTopPoint().GetDist( pFight->m_itEqMemory.m_pt );
	int iHisDistFromBattle = pTarg->GetTopPoint().GetDist( pFight->m_itEqMemory.m_pt );

	bool fCowardice = ( iMyDistFromBattle > iHisDistFromBattle );

	if ( fCowardice && ! pFight->IsMemoryTypes( MEMORY_IAGGRESSOR ))
	{
		// cowardice is ok if i was attacked.
		return;
	}
	Attacker_Delete(pTarg, false, ATTACKER_CLEAR_DISTANCE );

	SysMessagef( fCowardice ?
		g_Cfg.GetDefaultMsg( DEFMSG_COWARD_1 ) :
		g_Cfg.GetDefaultMsg( DEFMSG_COWARD_2 ), static_cast<LPCTSTR>(pTarg->GetName()));

	// Lose some fame.
	if ( fCowardice )
	{
		Noto_Fame( -1 );
	}
}

bool CChar::Memory_Fight_OnTick( CItemMemory * pMemory )
{
	ADDTOCALLSTACK("CChar::Memory_Fight_OnTick");
	// Check on the status of the fight.
	// return: false = delete the memory completely.
	//  true = skip it.

	ASSERT(pMemory);
	CChar * pTarg = pMemory->m_uidLink.CharFind();
	if ( pTarg == NULL )
		return( false );	// They are gone for some reason ?

	INT64 elapsed = Attacker_GetElapsed(Attacker_GetID(pTarg));
	// Attacker.Elapsed = -1 means no combat end for this attacker.
	// g_Cfg.m_iAttackerTimeout = 0 means attackers doesnt decay. (but cleared when the attacker is killed or the char dies)

	if ( GetDist(pTarg) > UO_MAP_VIEW_RADAR || ( g_Cfg.m_iAttackerTimeout != 0 && elapsed > static_cast<int>(g_Cfg.m_iAttackerTimeout) && elapsed == -1 ) )
	{
		Memory_Fight_Retreat( pTarg, pMemory );
clearit:
		Memory_ClearTypes( pMemory, MEMORY_FIGHT|MEMORY_IAGGRESSOR );
		return( true );
	}

	INT64 iTimeDiff = - g_World.GetTimeDiff( pMemory->GetTimeStamp() );

	// If am fully healthy then it's not much of a fight.
	if ( iTimeDiff > 60*60*TICK_PER_SEC )
		goto clearit;
	if ( pTarg->GetHealthPercent() >= 100 && iTimeDiff > 2*60*TICK_PER_SEC )
		goto clearit;

	pMemory->SetTimeout(20*TICK_PER_SEC);
	return( true );	// reschedule it.
}

void CChar::Memory_Fight_Start( const CChar * pTarg )
{
	ADDTOCALLSTACK("CChar::Memory_Fight_Start");
	// I am attacking this creature.
	// i might be the aggressor or just retaliating.
	// This is just the "Intent" to fight. Maybe No damage done yet.

	ASSERT(pTarg);
	if ( Fight_IsActive() && m_Act_Targ == pTarg->GetUID())
	{
		// quick check that things are ok.
		return;
	}

	WORD MemTypes;
	CItemMemory * pMemory = Memory_FindObj( pTarg );
	if ( pMemory == NULL )
	{
		// I have no memory of them yet.
		// There was no fight. Am I the aggressor ?
		CItemMemory * pTargMemory = pTarg->Memory_FindObj( this );
		if ( pTargMemory != NULL )	// My target remembers me.
		{
			if ( pTargMemory->IsMemoryTypes( MEMORY_IAGGRESSOR ))
				MemTypes = MEMORY_HARMEDBY;
			else if ( pTargMemory->IsMemoryTypes( MEMORY_HARMEDBY|MEMORY_SAWCRIME|MEMORY_AGGREIVED ))
				MemTypes = MEMORY_IAGGRESSOR;
			else
				MemTypes = 0;
		}
		else
		{
			// Hmm, I must have started this i guess.
			MemTypes = MEMORY_IAGGRESSOR;
		}
		pMemory = Memory_CreateObj( pTarg, MEMORY_FIGHT|MEMORY_WAR_TARG|MemTypes );
	}
	else
	{
		// I have a memory of them.
		bool fMemPrvType = pMemory->IsMemoryTypes(MEMORY_WAR_TARG);
		if ( fMemPrvType )
			return;
		if ( pMemory->IsMemoryTypes(MEMORY_HARMEDBY|MEMORY_SAWCRIME|MEMORY_AGGREIVED))
		{
			// I am defending myself rightly.
			MemTypes = 0;
		}
		else
		{
			MemTypes = MEMORY_IAGGRESSOR;
		}
		// Update the fights status
		Memory_AddTypes( pMemory, MEMORY_FIGHT|MEMORY_WAR_TARG|MemTypes );
	}
	Attacker_Add(const_cast<CChar*>(pTarg));
	if ( IsClient())
	{
		// This may be a useless command. How do i say the fight is over ?
		// This causes the funny turn to the target during combat !
		new PacketSwing(GetClient(), pTarg);
	}
	else
	{
		if ( m_pNPC && m_pNPC->m_Brain == NPCBRAIN_BERSERK ) // it will attack everything.
			return;
	}

	HUE_TYPE pHue = static_cast<HUE_TYPE>(g_Exp.m_VarDefs.GetKeyNum("EMOTE_DEF_COLOR"));
	FONT_TYPE pFont = static_cast<FONT_TYPE>(g_Exp.m_VarDefs.GetKeyNum("EMOTE_DEF_FONT"));
	bool bUnicode = (g_Exp.m_VarDefs.GetKeyNum("EMOTE_DEF_UNICODE",true) != 0);

	char *z = NULL;
	if ( GetTopSector()->GetCharComplexity() < 7 )
	{
		z = Str_GetTemp();
		sprintf(z, g_Cfg.GetDefaultMsg(DEFMSG_COMBAT_ATTACKO), GetName(), pTarg->GetName());
		UpdateObjMessage(z, NULL, pTarg->GetClient(), pHue, TALKMODE_EMOTE, pFont, bUnicode);
	}

	if ( pTarg->IsClient() && pTarg->CanSee(this))
	{
		if ( !z ) z = Str_GetTemp();
		sprintf(z, g_Cfg.GetDefaultMsg(DEFMSG_COMBAT_ATTACKS), GetName());
		pTarg->GetClient()->addBarkParse(z, this, pHue, TALKMODE_EMOTE, pFont, bUnicode);
	}
}

//********************************************************

SKILL_TYPE CChar::Fight_GetWeaponSkill() const
{
	ADDTOCALLSTACK("CChar::Fight_GetWeaponSkill");
	// What sort of weapon am i using?
	CItem * pWeapon = m_uidWeapon.ItemFind();
	if ( pWeapon == NULL )
		return( SKILL_WRESTLING );
	return( pWeapon->Weapon_GetSkill());
}

bool CChar::Fight_IsActive() const
{
	ADDTOCALLSTACK("CChar::Fight_IsActive");
	// Am i in an active fight mode ?
	if ( ! IsStatFlag(STATF_War))
		return( false );

	SKILL_TYPE iSkillActive		= Skill_GetActive();
	switch ( iSkillActive )
	{
		case SKILL_ARCHERY:
		case SKILL_FENCING:
		case SKILL_MACEFIGHTING:
		case SKILL_SWORDSMANSHIP:
		case SKILL_WRESTLING:
		case SKILL_THROWING:
			return( true );

		default:
			break;
	}

	if ( iSkillActive == Fight_GetWeaponSkill() )
		return true;

	return g_Cfg.IsSkillFlag( iSkillActive, SKF_FIGHT );
}

int CChar::Fight_CalcDamage( const CItem * pWeapon, SKILL_TYPE skill, bool bNoRandom ) const
{
	ADDTOCALLSTACK("CChar::Fight_CalcDamage");
	// STR bonus on close combat weapons.
	// and DEX bonus on archery weapons
	// A random value of the bonus is added (0-10%)

	if ( m_pNPC &&
		m_pNPC->m_Brain == NPCBRAIN_GUARD &&
		g_Cfg.m_fGuardsInstantKill )
	{
		return( 20000 );	// swing made.
	}

	int iDmg = 1;

	if (!IsSetCombatFlags(COMBAT_OSIDAMAGEMOD))
	{
		int iDmgAdj;
		if ( !g_Cfg.IsSkillRanged(skill) )
			iDmgAdj = bNoRandom ? Stat_GetAdjusted(STAT_STR) : Calc_GetRandVal( Stat_GetAdjusted(STAT_STR));
		else
			iDmgAdj = bNoRandom ? Stat_GetAdjusted(STAT_DEX) : Calc_GetRandVal( Stat_GetAdjusted(STAT_DEX));
		iDmg += iDmgAdj / 10;
	}

	if ( pWeapon != NULL )
	{
		iDmg += pWeapon->Weapon_GetAttack(bNoRandom);
	}
	else
	{
		// Pure wrestling damage.
		// Base type attack for our body. claws/etc
		CCharBase * pCharDef = Char_GetDef();
		ASSERT(pCharDef);

		iDmg += pCharDef->m_attackBase;
		if ( bNoRandom )
			iDmg += pCharDef->m_attackRange;
		else
			iDmg += Calc_GetRandVal( pCharDef->m_attackRange );
	}

	return( iDmg );
}

void CChar::Fight_ResetWeaponSwingTimer()
{
	ADDTOCALLSTACK("CChar::Fight_ResetWeaponSwingTimer");
	if ( Fight_IsActive())
	{
		// The target or the weapon might have changed.
		// So restart the skill
		Skill_Start( Fight_GetWeaponSkill() );
	}
}

int CChar::Fight_GetWeaponSwingTimer()
{
	ADDTOCALLSTACK("CChar::Fight_GetWeaponSwingTimer");
	// We have just equipped the weapon or gone into War mode.
	// Set the swing timer for the weapon or on us for .
	//   m_atFight.m_War_Swing_State = WAR_SWING_EQUIPPING or WAR_SWING_SWINGING;
	// RETURN:
	//  tenths of a sec. TICK_PER_SEC

	return( g_Cfg.Calc_CombatAttackSpeed( this, m_uidWeapon.ItemFind()));
}

void CChar::Fight_ClearAll()
{
	ADDTOCALLSTACK("CChar::Fight_ClearAll");
	// clear all my active targets. Toggle out of war mode.
	// Should I add @CombatEnd trigger here too?
	CItem * pItem = GetContentHead();
	for ( ; pItem != NULL; pItem = pItem->GetNext())
	{
		if ( ! pItem->IsMemoryTypes(MEMORY_WAR_TARG))
			continue;
		Memory_ClearTypes( STATIC_CAST <CItemMemory *>(pItem), MEMORY_WAR_TARG );
	}
	Attacker_Clear();

	// Our target is gone.
	StatFlag_Clear( STATF_War );

	if ( Fight_IsActive() )
	{
		Skill_Start( SKILL_NONE );
		m_Act_Targ.InitUID();
	}

	UpdateModeFlag();
}

CChar * CChar::Fight_FindBestTarget()
{
	ADDTOCALLSTACK("CChar::Fight_FindBestTarget");
	// If i am an NPC with no more targets then drop out of war mode.
	// RETURN:
	//  number of targets.
	if ( Attacker() )
		return Attacker_FindBestTarget((g_Cfg.m_iNpcAi&NPC_AI_THREAT && !m_pPlayer));
	/*else
	{
		SKILL_TYPE skillWeapon = Fight_GetWeaponSkill();
		int iClosest = INT_MAX;	// closest

		const CItem * pItem = GetContentHead();
		for ( ; pItem != NULL; pItem = pItem->GetNext())
		{
			if ( ! pItem->IsMemoryTypes(MEMORY_WAR_TARG))
				continue;
			// check that the item is actually a memory item
			const CItemMemory * pMemory = dynamic_cast <const CItemMemory *>(pItem);
			if ( pMemory == NULL )
				continue;

			pChar = pItem->m_uidLink.CharFind();
			if ( pChar == NULL)
				continue;

			int iDist = GetDist(pChar);

			if ( g_Cfg.IsSkillRanged( skillWeapon ) )
			{
				// archery dist is different.
				if ( iDist < g_Cfg.m_iArcheryMinDist || iDist > g_Cfg.m_iArcheryMaxDist )
					continue;
				if ( m_Act_Targ == pChar->GetUID())	// alternate.
					continue;
				return( pChar );
			}
			else if ( iDist < iClosest )
			{
				// ??? in the npc case. can i actually reach this target ?
				pClosest = pChar;
				iClosest = iDist;
			}
		}
	}*/
	return NULL;
}

bool CChar::Fight_Clear(const CChar *pChar, bool bForced)
{
	ADDTOCALLSTACK("CChar::Fight_Clear");
	// I no longer want to attack this char.
	if ( ! pChar )
		return false;

	if (Attacker_Delete(const_cast<CChar*>(pChar), bForced, ATTACKER_CLEAR_FORCED) == false)
		return false;

	// Go to my next target.
	pChar = Fight_FindBestTarget();
	if ( pChar )
		Fight_Attack(pChar);
	else if ( !m_pPlayer )
		Fight_ClearAll();

	return (pChar != NULL);	// I did not know about this ?
}

bool CChar::Fight_Attack( const CChar * pCharTarg, bool btoldByMaster )
{
	ADDTOCALLSTACK("CChar::Fight_Attack");
	// We want to attack some one.
	// But they won't notice til we actually hit them.
	// This is just my intent.
	// RETURN:
	//  true = new attack is accepted.

	if ( pCharTarg == NULL ||
		pCharTarg == this ||
		! CanSee(pCharTarg) ||
		pCharTarg->IsStatFlag( STATF_DEAD ) ||
		pCharTarg->IsDisconnected() ||
		IsStatFlag( STATF_DEAD ))
	{
		// Not a valid target.
		Fight_Clear( pCharTarg, true );
		return( false );
	}

	if ( g_Cfg.m_fAttackingIsACrime == TRUE)
	{
		CChar * pTarg = const_cast<CChar*>(pCharTarg);
		if ( pTarg->Noto_GetFlag( this ) == NOTO_GOOD && pTarg->Attacker_GetID( this ) < 0)
		{
			if ( IsClient())
			{
				// I decide if this is a crime.
				pTarg->OnNoticeCrime( this, pTarg );
			}
			else
			{
				// If it is a pet then this a crime others can report.
				CChar * pCharMark = IsStatFlag(STATF_Pet) ? NPC_PetGetOwner() : this;
				CheckCrimeSeen( SKILL_NONE, pCharMark, NULL, NULL );
			}
		}
	}
	INT64 threat = 0;
	if ( btoldByMaster )
		threat = 1000 + Attacker_GetHighestThreat();

	if ((m_Act_Targ != pCharTarg->GetUID()) && (( IsTrigUsed(TRIGGER_ATTACK) ) || ( IsTrigUsed(TRIGGER_CHARATTACK) )))
	{
		CScriptTriggerArgs Args;
		Args.m_iN1 = threat;
		if (OnTrigger(CTRIG_Attack, const_cast<CChar *>(pCharTarg), &Args) == TRIGRET_RET_TRUE)
			return false;
		threat = Args.m_iN1;
	}

	if ( GetPrivLevel() <= PLEVEL_Guest &&
		pCharTarg->m_pPlayer &&
		pCharTarg->GetPrivLevel() > PLEVEL_Guest )
	{
		SysMessageDefault( DEFMSG_GUEST );
		Fight_Clear( pCharTarg );
		return( false );
	}
	if ( Attacker_Add( const_cast<CChar*>( pCharTarg ), threat ) == false )
		return( false );
	// Record the start of the fight.
	Memory_Fight_Start( pCharTarg );

	// I am attacking. (or defending)
	StatFlag_Set( STATF_War );


	// Skill interruption ?
	SKILL_TYPE skillWeapon = Fight_GetWeaponSkill();
	SKILL_TYPE skillActive = Skill_GetActive();

	if ( skillActive == skillWeapon && m_Act_Targ == pCharTarg->GetUID() )
		return true;

	if ( IsSkillMagic(skillActive) )
	{
		int skill;
		const CSpellDef* pSpellDef = g_Cfg.GetSpellDef(m_atMagery.m_Spell);
		if ( pSpellDef != NULL && pSpellDef->GetPrimarySkill(&skill, NULL) )
		{
			int iInterrupt = pSpellDef->m_Interrupt.GetLinear( Skill_GetBase(static_cast<SKILL_TYPE>(skill)) );
			if ( Calc_GetRandVal( 1000 ) >= iInterrupt )
				return true;
		}
	}
	CChar * pTarget = const_cast<CChar*>(pCharTarg);
	if (!m_pPlayer && !btoldByMaster)	// We call for FindBestTarget when this CChar is not a player and was not commanded to attack, otherwise it attack directly.
	{
		pTarget = Fight_FindBestTarget();
		if (!pTarget)
			return false;
	}
	m_Act_Targ =  pTarget->GetUID();
	Skill_Start( skillWeapon );

	return( true );
}

bool CChar::Fight_AttackNext()
{
	ADDTOCALLSTACK("CChar::Fight_AttackNext");
	return Fight_Attack( Fight_FindBestTarget());
}

void CChar::Fight_HitTry()
{
	ADDTOCALLSTACK("CChar::Fight_HitTry");
	// A timer has expired so try to take a hit.
	// I am ready to swing or already swinging.
	// but i might not be close enough.
	// RETURN:
	//  false = no swing taken
	//  true = continue fighting

	ASSERT( Fight_IsActive());
	ASSERT( m_atFight.m_War_Swing_State == WAR_SWING_READY || m_atFight.m_War_Swing_State == WAR_SWING_SWINGING );

	CChar * pCharTarg = m_Act_Targ.CharFind();
	if ( pCharTarg == NULL )
	{
		// Might be dead ? Clear this.
		// move to my next target.
		Fight_AttackNext();
		return;
	}


	// Try to hit my target. I'm ready.
	switch ( Fight_Hit( pCharTarg ))
	{
		case WAR_SWING_INVALID:	// target is invalid.
			Fight_Clear( pCharTarg );
			Fight_AttackNext();
			return;
		case WAR_SWING_EQUIPPING:
			// Assume I want to continue hitting
			// Swing again. (start swing delay)
			// same target.
			{
				SKILL_TYPE skill = Skill_GetActive();
				Skill_Cleanup();	// Smooth transition = not a cancel of skill.
				Skill_Start( skill );
			}
			return;
		case WAR_SWING_READY:	// probably too far away. can't take my swing right now.
			// Try for a diff target ?
			Fight_AttackNext();
			return;
		case WAR_SWING_SWINGING:	// must come back here again to complete.
			return;
		default:
			break;
	}

	ASSERT(0);
}
bool CChar::Attacker_Add( CChar * pChar, INT64 threat )
{
	ADDTOCALLSTACK("CChar::Attacker_Add");
	CGrayUID uid = static_cast<CGrayUID>(pChar->GetUID());
	if  ( m_lastAttackers.size() )	// Must only check for existing attackers if there are any attacker already.
	{
		for (std::vector<LastAttackers>::iterator it = m_lastAttackers.begin(); it != m_lastAttackers.end(); ++it)
		{
			LastAttackers & refAttacker = *it;
			if ( refAttacker.charUID == uid )
			{
				//Found one, no actions needed so we skip
				return true;
			}
		}
	}else if ( IsTrigUsed(TRIGGER_COMBATSTART) )
	{
		TRIGRET_TYPE tRet = OnTrigger(CTRIG_CombatStart,pChar,0);
		if ( tRet == TRIGRET_RET_TRUE )
			return false;
	}
	CScriptTriggerArgs Args;
	bool fIgnore = false;
	Args.m_iN1 = threat;
	Args.m_iN2 = fIgnore;
	if ( IsTrigUsed(TRIGGER_COMBATADD) )
	{
		TRIGRET_TYPE tRet = OnTrigger(CTRIG_CombatAdd,pChar,&Args);
		if ( tRet == TRIGRET_RET_TRUE )
			return false;
		threat = Args.m_iN1;
		fIgnore = (Args.m_iN2 != 0);
	}

	LastAttackers attacker;
	attacker.amountDone = 0;
	attacker.charUID = uid;
	attacker.elapsed = 0;
	if ( m_pPlayer )
		attacker.threat = 0;
	else
		attacker.threat = threat;
	attacker.ignore = fIgnore;
	m_lastAttackers.push_back(attacker);
	return true;
}

CChar * CChar::Attacker_FindBestTarget( bool bUseThreat )
{
	ADDTOCALLSTACK("CChar::Attacker_FindBestTarget");
	if ( !Attacker() )
		return NULL;
	SKILL_TYPE skillWeapon = Fight_GetWeaponSkill();
	int iClosest = INT_MAX;	// closest

	CChar * pChar = NULL;
	CChar * pClosest = NULL;
	if ( ! m_lastAttackers.size() )
		return pChar;

	INT64 threat = 0;
	int count = 0;

	for ( std::vector<LastAttackers>::iterator it = m_lastAttackers.begin(); it != m_lastAttackers.end(); it++)
	{
		LastAttackers & refAttacker = *it;
		CChar * pChar = static_cast<CChar*>( static_cast<CGrayUID>( refAttacker.charUID ).CharFind() );
	
		if ( pChar == NULL )
			continue;
		if (refAttacker.ignore)
			continue;
		if ( pClosest == NULL )
			pClosest = pChar;
		int iDist = GetDist(pChar);
		
		// Main priority is the Threat
		// but we only add it if it meets some requirements: target must not be far than view size and must be in LOS.

		if ( g_Cfg.IsSkillRanged( skillWeapon ) )
		{
			// archery dist is different.
			if ( iDist < g_Cfg.m_iArcheryMinDist || iDist > g_Cfg.m_iArcheryMaxDist )
				continue;
			if ( m_Act_Targ == pChar->GetUID())	// alternate.
				continue;
		} 
		if ( iDist > UO_MAP_VIEW_SIGHT )
			continue;

		if ( ! CanSeeLOS( pChar ) )
			continue;

		if ( bUseThreat && threat < refAttacker.threat)
		{
			// So if we reached here ... this target has more threat than the others and meets the reqs.
			pClosest = pChar;
			iClosest = iDist;
			threat = refAttacker.threat;

		}
		else if (iDist < iClosest)
		{
			// ??? in the npc case. can i actually reach this target ?
			pClosest = pChar;
			iClosest = iDist;
		}
		count ++;
	}
	return ( pClosest ) ? pClosest : pChar;
}

INT64 CChar::Attacker_GetDam( int id)
{
	ADDTOCALLSTACK("CChar::Attacker_GetDam");
	if ( ! m_lastAttackers.size() )
		return -1;
	if (  static_cast<int>(m_lastAttackers.size()) <= id )
		return -1;
	LastAttackers & refAttacker = m_lastAttackers.at(id);
	return refAttacker.amountDone;
}

INT64 CChar::Attacker_GetElapsed( int id)
{
	ADDTOCALLSTACK("CChar::Attacker_GetElapsed");
	if ( ! m_lastAttackers.size() )
		return -1;
	if ( static_cast<int>(m_lastAttackers.size()) <= id )
		return -1;
	if ( id < 0 )
		return -1;
	LastAttackers & refAttacker = m_lastAttackers.at(id);
	return refAttacker.elapsed;
}

INT64 CChar::Attacker_GetThreat( int id)
{
	ADDTOCALLSTACK("CChar::Attacker_GetThreat");
	if ( ! m_lastAttackers.size() )
		return -1;
	if ( static_cast<int>(m_lastAttackers.size()) <= id )
		return -1;
	LastAttackers & refAttacker = m_lastAttackers.at(id);
	return refAttacker.threat ? refAttacker.threat : 0;
}

INT64 CChar::Attacker_GetHighestThreat()
{
	if ( !m_lastAttackers.size() )
		return -1;
	INT64 highThreat = 0;
	for ( unsigned int count = 0; count < m_lastAttackers.size(); count++ )
	{
		LastAttackers & refAttacker = m_lastAttackers.at(count);
		if ( refAttacker.threat > highThreat )
			highThreat = refAttacker.threat;
	}
	return highThreat;
}

void CChar::Attacker_SetElapsed( CChar * pChar, INT64 value)
{
	ADDTOCALLSTACK("CChar::Attacker_SetElapsed(CChar)");

	return Attacker_SetElapsed( Attacker_GetID(pChar), value);

}


void CChar::Attacker_SetElapsed( int pChar, INT64 value)
{
	ADDTOCALLSTACK("CChar::Attacker_SetElapsed(int)");
	if (pChar < 0)
		return;
	if ( ! m_lastAttackers.size() )
		return;
	if ( static_cast<int>(m_lastAttackers.size()) <= pChar )
		return;
	LastAttackers & refAttacker = m_lastAttackers.at( pChar );
	refAttacker.elapsed = value;
}

void CChar::Attacker_SetDam( CChar * pChar, INT64 value)
{
	ADDTOCALLSTACK("CChar::Attacker_SetDam(CChar)");

	return Attacker_SetDam( Attacker_GetID(pChar), value );
}


void CChar::Attacker_SetDam( int pChar, INT64 value)
{
	ADDTOCALLSTACK("CChar::Attacker_SetDam(int)");
	if (pChar < 0)
		return;
	if ( ! m_lastAttackers.size() )
		return;
	if ( static_cast<int>(m_lastAttackers.size()) <= pChar )
		return;
	LastAttackers & refAttacker = m_lastAttackers.at( pChar );
	refAttacker.amountDone = value;
}

void CChar::Attacker_SetThreat(CChar * pChar, INT64 value)
{
	ADDTOCALLSTACK("CChar::Attacker_SetThreat(CChar)");

	return Attacker_SetThreat(Attacker_GetID(pChar), value);
}


void CChar::Attacker_SetThreat(int pChar, INT64 value)
{
	ADDTOCALLSTACK("CChar::Attacker_SetThreat(int)");
	if (pChar < 0)
		return;
	if (m_pPlayer)
		return;
	if (!m_lastAttackers.size())
		return;
	if (static_cast<int>(m_lastAttackers.size()) <= pChar)
		return;
	LastAttackers & refAttacker = m_lastAttackers.at(pChar);
	refAttacker.threat = value;
}

void CChar::Attacker_SetIgnore(CChar * pChar, bool fIgnore)
{
	ADDTOCALLSTACK("CChar::Attacker_SetIgnore(CChar)");

	return Attacker_SetIgnore(Attacker_GetID(pChar), fIgnore);
}


void CChar::Attacker_SetIgnore(int pChar, bool fIgnore)
{
	ADDTOCALLSTACK("CChar::Attacker_SetIgnore(int)");
	if (pChar < 0)
		return;
	if (!m_lastAttackers.size())
		return;
	if (static_cast<int>(m_lastAttackers.size()) <= pChar)
		return;
	LastAttackers & refAttacker = m_lastAttackers.at(pChar);
	refAttacker.ignore = fIgnore;
}

bool CChar::Attacker_GetIgnore(CChar * pChar)
{
	ADDTOCALLSTACK("CChar::Attacker_GetIgnore(CChar)");
	return Attacker_GetIgnore(Attacker_GetID(pChar));
}

bool CChar::Attacker_GetIgnore(int id)
{
	ADDTOCALLSTACK("CChar::Attacker_GetIgnore(int)");
	if (!m_lastAttackers.size())
		return false;
	if (static_cast<int>(m_lastAttackers.size()) <= id)
		return false;
	if (id < 0)
		return false;
	LastAttackers & refAttacker = m_lastAttackers.at(id);
	return (refAttacker.elapsed != 0);
}

void CChar::Attacker_Clear()
{
	ADDTOCALLSTACK("CChar::Attacker_Clear");
	if ( IsTrigUsed(TRIGGER_COMBATEND) )
		OnTrigger(CTRIG_CombatEnd,this,0);
	m_lastAttackers.clear();
		// Our target is gone.
	StatFlag_Clear( STATF_War );

	if ( Fight_IsActive() )
	{
		Skill_Start( SKILL_NONE );
		m_Act_Targ.InitUID();
	}

	UpdateModeFlag();
}

int CChar::Attacker_GetID( CChar * pChar )
{
	ADDTOCALLSTACK("CChar::Attacker_GetID(CChar)");
	if ( !pChar )
		return -1;
	if ( ! m_lastAttackers.size() )
		return -1;
	int count = 0;
	for ( std::vector<LastAttackers>::iterator it = m_lastAttackers.begin(); it != m_lastAttackers.end(); it++)
	{
		LastAttackers & refAttacker = m_lastAttackers.at(count);
		CGrayUID uid = refAttacker.charUID;
		if ( ! uid || !uid.CharFind() )
			continue;
		CChar * pMe = uid.CharFind()->GetChar();
		if ( ! pMe )
			continue;
		if ( pMe == pChar )
			return count;
	
		count++;
	}
	return -1;
}

int CChar::Attacker_GetID( CGrayUID pChar )
{
	ADDTOCALLSTACK("CChar::Attacker_GetID(CGrayUID)");
	return Attacker_GetID( pChar.CharFind()->GetChar() );
}

CChar * CChar::Attacker_GetUID( int index )
{
	ADDTOCALLSTACK("CChar::Attacker_GetUID");
	if ( ! m_lastAttackers.size() )
		return NULL;
	if ( static_cast<int>(m_lastAttackers.size()) <= index )
		return NULL;
	LastAttackers & refAttacker = m_lastAttackers.at(index);
	CChar * pChar = static_cast<CChar*>( static_cast<CGrayUID>( refAttacker.charUID ).CharFind() );
	return pChar;
}
bool CChar::Attacker_Delete( int index, bool bForced, ATTACKER_CLEAR_TYPE type )
{
	ADDTOCALLSTACK("CChar::Attacker_Delete(int)");
	if ( ! m_lastAttackers.size() )
		return false;
	if ( index < 0 )
		return false;
	if ( static_cast<int>(m_lastAttackers.size()) <= index )
		return false;
	LastAttackers & refAttacker = m_lastAttackers.at(index);
	CChar * pChar = static_cast<CGrayUID>(refAttacker.charUID).CharFind();
	if ( ! pChar )
		return false;
	
	if ( IsTrigUsed(TRIGGER_COMBATDELETE) )
	{
		CScriptTriggerArgs Args;
		Args.m_iN1 = static_cast<int>(bForced);
		Args.m_iN2 = (int)type;
		TRIGRET_TYPE tRet = OnTrigger(CTRIG_CombatDelete,pChar,&Args);
		if ( tRet == TRIGRET_RET_TRUE  &&  Args.m_iN1 == 1 )
			return false;
	}
	std::vector<LastAttackers>::iterator it = m_lastAttackers.begin() + index;
	CItemMemory *pFight = Memory_FindObj(pChar->GetUID());	// My memory of the fight.
	if ( pFight )
	{
		Memory_ClearTypes(pFight, MEMORY_WAR_TARG);
	}
	m_lastAttackers.erase(it);

	if ( ! m_lastAttackers.size() )
		Attacker_Clear();
	return true;
}

bool CChar::Attacker_Delete(CChar * pChar, bool bForced, ATTACKER_CLEAR_TYPE type)
{		
	ADDTOCALLSTACK("CChar::Attacker_Delete(CChar)");
	if ( !pChar )
		return false;
	if ( ! m_lastAttackers.size() )
		return false;
	return Attacker_Delete( Attacker_GetID( pChar), bForced, type );
}

void CChar::Attacker_RemoveChar()
{
	ADDTOCALLSTACK("CChar::Attacker_RemoveChar");
	if ( m_lastAttackers.size() )
	{
		for ( int count = 0 ; count < static_cast<int>(m_lastAttackers.size()); count++)
		{
			LastAttackers & refAttacker = m_lastAttackers.at(count);
			CChar * pSrc = static_cast<CGrayUID>(refAttacker.charUID).CharFind();
			if ( !pSrc )
				continue;
			pSrc->Attacker_Delete(pSrc->Attacker_GetID(this), false, ATTACKER_CLEAR_REMOVEDCHAR);
		}
	}
}

void CChar::Attacker_CheckTimeout()
{
	if ( m_lastAttackers.size() )
	{
		for ( int count = 0 ; count < static_cast<int>(m_lastAttackers.size()); count++)
		{
			LastAttackers & refAttacker = m_lastAttackers.at(count);
			if ( ( ++(refAttacker.elapsed) > g_Cfg.m_iAttackerTimeout ) && ( g_Cfg.m_iAttackerTimeout > 0  ) )
			{
				Attacker_Delete(count, true, ATTACKER_CLEAR_ELAPSED);
				break;
			}
		}
	}
}

int CChar::CalcFightRange( CItem * pWeapon )
{
	ADDTOCALLSTACK("CChar::CalcFightRange");

	int iCharRange = RangeL();
	int iWeaponRange = pWeapon ? pWeapon->RangeL() : 0;

	return ( maximum(iCharRange , iWeaponRange) );
}


WAR_SWING_TYPE CChar::Fight_Hit( CChar * pCharTarg )
{
	ADDTOCALLSTACK("CChar::Fight_Hit");
	// Attempt to hit our target.
	// pCharTarg = the target.
	//  NOT - WAR_SWING_EQUIPPING = 0,	// we are recoiling our weapon.
	//  WAR_SWING_READY,			// we can swing at any time.
	//  WAR_SWING_SWINGING,			// we are swinging our weapon.
	// RETURN:
	//  WAR_SWING_INVALID = target is invalid
	//  WAR_SWING_EQUIPPING = swing made.
	//  WAR_SWING_READY = can't take my swing right now. but i'm ready
	//  WAR_SWING_SWINGING = taking my swing now.
	
	int iTyp = DAMAGE_HIT_BLUNT;

	if ( IsTrigUsed( TRIGGER_HITCHECK ) )
	{
		CScriptTriggerArgs pArgs;
		pArgs.m_iN1 = m_atFight.m_War_Swing_State;
		pArgs.m_iN2 = iTyp;
		TRIGRET_TYPE tRet;
		tRet = OnTrigger( CTRIG_HitCheck, pCharTarg, &pArgs);
		if ( tRet == TRIGRET_RET_TRUE )
			return (WAR_SWING_TYPE)pArgs.m_iN1;
		else if ( tRet == -1 )
			return WAR_SWING_INVALID;
		m_atFight.m_War_Swing_State = (WAR_SWING_TYPE)pArgs.m_iN1;
		iTyp = static_cast<int>(pArgs.m_iN2);

		if (( m_atFight.m_War_Swing_State == WAR_SWING_SWINGING) && ( iTyp & DAMAGE_FIXED ) )
		{
			if (tRet == 2)
				return WAR_SWING_EQUIPPING;

			if ( iTyp == DAMAGE_HIT_BLUNT )	// If type did not change in the trigger, default iTyp is set.
			{
				CItem	*pWeapon		= m_uidWeapon.ItemFind();
				CItemBase * pWeaponDef	= NULL;
				if ( pWeapon )
				{
					pWeaponDef = pWeapon->Item_GetDef();
					// set damage type according to weapon type
					switch (pWeaponDef->GetType())
					{
						case IT_WEAPON_SWORD:
						case IT_WEAPON_AXE:
						case IT_WEAPON_THROWING:
							iTyp |= DAMAGE_HIT_SLASH;
							break;
						case IT_WEAPON_FENCE:
						case IT_WEAPON_BOW:
						case IT_WEAPON_XBOW:
							iTyp |= DAMAGE_HIT_PIERCE;
							break;
						default:
							break;
					}

					// look for override TAG on the specific weapon
					CVarDefCont * pDamTypeOverride = pWeapon->GetKey("OVERRIDE.DAMAGETYPE",true);
					if (pDamTypeOverride)
						iTyp = static_cast<int>(pDamTypeOverride->GetValNum());
				}
			}
			if ( iTyp & DAMAGE_FIXED )
				iTyp = iTyp &~ DAMAGE_FIXED;
			pCharTarg->OnTakeDamage( Fight_CalcDamage( m_uidWeapon.ItemFind(), Skill_GetActive() ), this, iTyp );

			return( WAR_SWING_EQUIPPING );	// Made our full swing.
		}
	}
	if ( !pCharTarg || ( pCharTarg == this ) )
		return WAR_SWING_INVALID;

	//	Very basic check on possibility to hit
	if ( IsStatFlag(STATF_DEAD|STATF_Sleeping|STATF_Freeze|STATF_Stone) || 
			pCharTarg->IsStatFlag(STATF_DEAD|STATF_INVUL|STATF_Stone) ||
			( pCharTarg->Stat_GetVal(STAT_STR) < 1 ) )
		return WAR_SWING_INVALID;

	if ( m_pArea && m_pArea->IsFlag(REGION_FLAG_SAFE ) )
		return WAR_SWING_INVALID;

	if ( pCharTarg->m_pArea && pCharTarg->m_pArea->IsFlag(REGION_FLAG_SAFE) )
		return WAR_SWING_INVALID;

	int dist = GetTopDist3D(pCharTarg);
	if ( dist > UO_MAP_VIEW_RADAR )
	{
		if ( !IsSetCombatFlags(COMBAT_STAYINRANGE) || m_atFight.m_War_Swing_State != WAR_SWING_SWINGING )
			return( WAR_SWING_INVALID );

		return( WAR_SWING_EQUIPPING );
	}

	//	I am on ship. Should be able to combat only inside the ship to avoid free sea and
	//	ground characters hunting
	if ( !IsSetCombatFlags(COMBAT_ALLOWHITFROMSHIP) && ( m_pArea != pCharTarg->m_pArea ))
	{
		if ( m_pArea && m_pArea->IsFlag(REGION_FLAG_SHIP) )
		{
			SysMessageDefault( DEFMSG_COMBAT_OUTSIDESHIP );
			Skill_Start( SKILL_NONE );
			return( WAR_SWING_INVALID );
		}
		if ( pCharTarg->m_pArea && pCharTarg->m_pArea->IsFlag(REGION_FLAG_SHIP) )
		{
			SysMessageDefault( DEFMSG_COMBAT_INSIDESHIP );
			Skill_Start( SKILL_NONE );
			return( WAR_SWING_INVALID );
		}
	}

	if ( CanSee(pCharTarg) )
	{
		if ( (pCharTarg->m_pNPC && pCharTarg->IsStatFlag(STATF_Ridden) ) || !CanSeeLOS(pCharTarg, (g_Cfg.IsSkillRanged(Skill_GetActive()) ? LOS_NB_WINDOWS : 0x0) ) ) //Allow archery through a window
		{
			if ( !IsSetCombatFlags(COMBAT_STAYINRANGE) || m_atFight.m_War_Swing_State != WAR_SWING_SWINGING )
				return( WAR_SWING_READY );

			return( WAR_SWING_EQUIPPING );
		}
	}
	else
	{
		if ( !IsSetCombatFlags(COMBAT_STAYINRANGE) || m_atFight.m_War_Swing_State != WAR_SWING_SWINGING )
			return( WAR_SWING_READY );

		return( WAR_SWING_EQUIPPING );
	}

	//	conjured npc removed on guard hit
	if ( m_pNPC && ( m_pNPC->m_Brain == NPCBRAIN_GUARD ) && pCharTarg->m_pNPC && pCharTarg->IsStatFlag(STATF_Conjured) )
	{
		pCharTarg->Delete();
		return WAR_SWING_EQUIPPING;
	}

	//	fix of the bounce back effect with dir update for clients to be able to run in combat easily
	if ( g_Cfg.m_iCombatFlags&COMBAT_FACECOMBAT && IsClient() )
	{
		DIR_TYPE dirOpponent = GetDir(pCharTarg, m_dirFace);
		if (( dirOpponent != m_dirFace ) && ( dirOpponent != GetDirTurn(m_dirFace, -1) ) && ( dirOpponent != GetDirTurn(m_dirFace, 1) ))
			return WAR_SWING_READY;
	}

	CItem	*pWeapon		= m_uidWeapon.ItemFind();
	CItem	*pAmmo			= NULL;
	CItemBase * pWeaponDef	= NULL;
	if ( pWeapon )
	{
		pWeaponDef = pWeapon->Item_GetDef();
		// set damage type according to weapon type
		switch (pWeaponDef->GetType())
		{
			case IT_WEAPON_SWORD:
			case IT_WEAPON_AXE:
			case IT_WEAPON_THROWING:
				iTyp |= DAMAGE_HIT_SLASH;
				break;
			case IT_WEAPON_FENCE:
			case IT_WEAPON_BOW:
			case IT_WEAPON_XBOW:
				iTyp |= DAMAGE_HIT_PIERCE;
				break;
			default:
				break;
		}

		// look for override TAG on the specific weapon
		CVarDefCont * pDamTypeOverride = pWeapon->GetKey("OVERRIDE.DAMAGETYPE",true);
		if (pDamTypeOverride)
			iTyp = static_cast<int>(pDamTypeOverride->GetValNum());
	}


	SKILL_TYPE skill = Skill_GetActive();
	if ( g_Cfg.IsSkillRanged(skill) )
	{
		if (IsSetCombatFlags(COMBAT_PREHIT) && (m_atFight.m_War_Swing_State == WAR_SWING_READY))
		{
			INT64 diff = GetKeyNum("LastHit", true) - g_World.GetCurrentTime().GetTimeRaw();
			if (diff > 0)
			{
				diff = (diff > 50) ? 50 : diff;
				SetTimeout(diff);
				return(WAR_SWING_READY);
			}
		}
		// Archery type skill.
		int	iMinDist	= pWeapon ? pWeapon->RangeH() : g_Cfg.m_iArcheryMinDist;
		int	iMaxDist	= pWeapon ? pWeapon->RangeL() : g_Cfg.m_iArcheryMaxDist;

		if ( !iMaxDist || (iMinDist == 0 && iMaxDist == 1) )
			iMaxDist	= g_Cfg.m_iArcheryMaxDist;
		if ( !iMinDist )
			iMinDist	= g_Cfg.m_iArcheryMinDist;

		if ( dist > iMaxDist )
		{
			if ( !IsSetCombatFlags(COMBAT_STAYINRANGE) || m_atFight.m_War_Swing_State != WAR_SWING_SWINGING )
				return( WAR_SWING_READY );	// can't hit now.

			return( WAR_SWING_EQUIPPING );
		}

		if ( IsStatFlag( STATF_HasShield ))	// this should never happen.
		{
			SysMessageDefault( DEFMSG_ITEMUSE_BOW_SHIELD );
			Skill_Start( SKILL_NONE );
			return( WAR_SWING_INVALID );
		}

		if ( dist < iMinDist )
		{
			// ??? the bow is acting like a (poor) blunt weapon at this range?
			SysMessageDefault( DEFMSG_COMBAT_ARCH_TOOCLOSE );
			int iTime = Fight_GetWeaponSwingTimer();
			UpdateAnimate(GenerateAnimate(ANIM_ATTACK_1H_WIDE, false, false), false, false, iTime / TICK_PER_SEC);
			return( WAR_SWING_EQUIPPING );
		}

		Reveal();
		if ( ! IsSetCombatFlags(COMBAT_NODIRCHANGE) )
			UpdateDir( pCharTarg );

		// Consume the bolts/arrows
		CVarDefCont * pValue = pWeapon->GetDefKey("AMMOTYPE",true);
		CVarDefCont * pCont = pWeapon->GetDefKey("AMMOCONT",true);
		CVarDefCont * pAnim  = pWeapon->GetDefKey("AMMOANIM",true);
		CVarDefCont * pColor = pWeapon->GetDefKey("AMMOANIMHUE",true);
		CVarDefCont * pRender = pWeapon->GetDefKey("AMMOANIMRENDER",true);
		ITEMID_TYPE AmmoID;
		ITEMID_TYPE AmmoAnim;
		DWORD AmmoHue;
		DWORD AmmoRender;
		RESOURCE_ID_BASE rid;
		LPCTSTR t_Str;

		if ( pValue )
		{
			t_Str = pValue->GetValStr();
			rid = static_cast<RESOURCE_ID_BASE>(g_Cfg.ResourceGetID( RES_ITEMDEF, t_Str ));
		} else
			rid = pWeaponDef->m_ttWeaponBow.m_idAmmo;

		AmmoID = static_cast<ITEMID_TYPE>(rid.GetResIndex());

		if ( AmmoID )
		{
			if ( pCont )
			{
				//check for UID
				CGrayUID uidCont = static_cast<DWORD>(pCont->GetValNum());
				CItemContainer *pNewCont = dynamic_cast <CItemContainer*> (uidCont.ItemFind());
				if (!pNewCont) //if no UID, check for ITEMID_TYPE
				{
					t_Str = pCont->GetValStr();
					RESOURCE_ID_BASE rContid = static_cast<RESOURCE_ID_BASE>(g_Cfg.ResourceGetID( RES_ITEMDEF, t_Str ));
					ITEMID_TYPE ContID = static_cast<ITEMID_TYPE>(rContid.GetResIndex());
					if (ContID)
						pNewCont = dynamic_cast <CItemContainer*> (ContentFind( rContid ));
				}

				if (pNewCont)
					pAmmo = pNewCont->ContentFind( rid );
				else
					pAmmo = ContentFind( rid );
			}
			else
				pAmmo = ContentFind( rid );
			if ( m_pPlayer && pAmmo == NULL )
			{
				SysMessageDefault( DEFMSG_COMBAT_ARCH_NOAMMO );
				Skill_Start( SKILL_NONE );
				return( WAR_SWING_INVALID );
			}
		}

		if ( m_atFight.m_War_Swing_State == WAR_SWING_READY )
		{
			if (dist > iMaxDist)
				return WAR_SWING_READY;

			Reveal();
			if (!IsSetCombatFlags(COMBAT_NODIRCHANGE))
				UpdateDir(pCharTarg);

			// just start the bow animation.
			INT64 iTime = Fight_GetWeaponSwingTimer();
			ANIM_TYPE anim = GenerateAnimate(ANIM_ATTACK_WEAPON);
			unsigned char animDelay = static_cast<unsigned char>(iTime) / TICK_PER_SEC;
			if ( IsTrigUsed(TRIGGER_HITTRY) )
			{
				CScriptTriggerArgs	Args( iTime, 0, pWeapon );
				Args.m_VarsLocal.SetNum("Anim", (int)anim);
				Args.m_VarsLocal.SetNum("AnimDelay", animDelay);
				if ( OnTrigger( CTRIG_HitTry, pCharTarg, &Args ) == TRIGRET_RET_TRUE )
					return( WAR_SWING_READY );
				iTime = Args.m_iN1;
				anim = (ANIM_TYPE)Args.m_VarsLocal.GetKeyNum("Anim",false);
				animDelay = (unsigned char)Args.m_VarsLocal.GetKeyNum("AnimDelay", true);
				if (animDelay < 0)
					animDelay = 0;
			}

			m_atFight.m_War_Swing_State = WAR_SWING_SWINGING;
			m_atFight.m_fMoved	= 0;
			SetTimeout( iTime * 3 / 4 );	// try again sooner.
			if ( anim >= 0)
				UpdateAnimate( anim,false, false, animDelay);
			return( WAR_SWING_SWINGING );
		}

		// now use the ammo
		if ( pAmmo )
		{
			pAmmo->UnStackSplit( 1, this );
			pAmmo->Delete();	// Delete by default.
		}

		if ( pAnim )
		{
			t_Str = pAnim->GetValStr();
			rid = static_cast<RESOURCE_ID_BASE>(g_Cfg.ResourceGetID( RES_ITEMDEF, t_Str ));
			AmmoAnim = static_cast<ITEMID_TYPE>(rid.GetResIndex());
		} else
		{
			AmmoAnim = static_cast<ITEMID_TYPE>(pWeaponDef->m_ttWeaponBow.m_idAmmoX.GetResIndex());
		}

		if ( pColor )
		{
			AmmoHue = static_cast<unsigned long>(pColor->GetValNum());
		} else
		{
			AmmoHue = 0;
		}

		if ( pRender )
		{
			AmmoRender = static_cast<unsigned long>(pRender->GetValNum());
		} else
		{
			AmmoRender = 0;
		}

		pCharTarg->Effect( EFFECT_BOLT, AmmoAnim, this, 5, 16, false, AmmoHue, AmmoRender );
	}
	else
	{
		if ( IsSetCombatFlags(COMBAT_PREHIT) && ( m_atFight.m_War_Swing_State == WAR_SWING_READY ))
		{
			INT64 diff = GetKeyNum("LastHit", true) - g_World.GetCurrentTime().GetTimeRaw();
			if ( diff > 0 )
			{
				diff = ( diff > 50 )? 50: diff;
				SetTimeout( diff );
				return( WAR_SWING_READY );
			}
		}

		int	iMinDist	= pWeapon ? pWeapon->RangeH() : 0;
		int	iMaxDist	= CalcFightRange( pWeapon );

		if (( dist < iMinDist ) || ( dist > iMaxDist ))
		{
			if ( !IsSetCombatFlags(COMBAT_STAYINRANGE) || m_atFight.m_War_Swing_State != WAR_SWING_SWINGING )
				return( WAR_SWING_READY );

			return( WAR_SWING_EQUIPPING );
		}

		// A hand weapon of some sort.
		if ( m_atFight.m_War_Swing_State == WAR_SWING_READY )
		{
			if ( dist > iMaxDist )
				return WAR_SWING_READY;

			Reveal();
			if ( ! IsSetCombatFlags(COMBAT_NODIRCHANGE) )
				UpdateDir(pCharTarg);

			// We are swinging.
			INT64 iTime = Fight_GetWeaponSwingTimer();
			ANIM_TYPE anim = GenerateAnimate(ANIM_ATTACK_WEAPON);
			unsigned char animDelay = static_cast<unsigned char>(iTime) / TICK_PER_SEC;
			if (IsTrigUsed(TRIGGER_HITTRY))
			{
				CScriptTriggerArgs	Args(iTime, 0, pWeapon);
				Args.m_VarsLocal.SetNum("Anim", (int)anim);
				Args.m_VarsLocal.SetNum("AnimDelay", animDelay);
				if (OnTrigger(CTRIG_HitTry, pCharTarg, &Args) == TRIGRET_RET_TRUE)
					return(WAR_SWING_READY);
				iTime = Args.m_iN1;
				anim = (ANIM_TYPE)Args.m_VarsLocal.GetKeyNum("Anim", false);
				animDelay = (unsigned char)Args.m_VarsLocal.GetKeyNum("AnimDelay", true);
				if (animDelay < 0)
					animDelay = 0;
			}

			m_atFight.m_War_Swing_State = WAR_SWING_SWINGING;
			m_atFight.m_fMoved	= 0;

			if ( IsSetCombatFlags(COMBAT_PREHIT) )
			{
				SetKeyNum("LastHit", iTime + g_World.GetCurrentTime().GetTimeRaw());
				UpdateAnimate( anim, false, false, 0);
				SetTimeout( 1 );
			}
			else
			{
				SetTimeout( iTime/2 );	// try again sooner.
				if ( anim >= 0)
					UpdateAnimate( anim, false, false, animDelay );
			}
			return( WAR_SWING_SWINGING );
		}

		Reveal();
		if ( ! IsSetCombatFlags(COMBAT_NODIRCHANGE) )
			UpdateDir(pCharTarg);
	}

	// We made our swing. so we must recoil.
	m_atFight.m_War_Swing_State = WAR_SWING_EQUIPPING;
	m_atFight.m_fMoved	= 0;
	INT64 iTime = static_cast<INT64>(Fight_GetWeaponSwingTimer());
	SetTimeout(iTime/2);	// try again sooner.

	// Stamina for fighting. More stamina loss for more heavy weapons
	int iWeaponWeight = minimum((( pWeapon ) ? ( pWeapon->GetWeight()/WEIGHT_UNITS + 1 ) : 1 ), 10);
	if ( !Calc_GetRandVal(20 - iWeaponWeight) )
	{
		UpdateStatVal( STAT_DEX, -1 );
	}

	CVarDefCont * pTagStorage = NULL; 
	SOUND_TYPE iSnd = 0;

	// Check if we hit something;
	if ( IsTrigUsed(TRIGGER_HITMISS) )
	{
		if ( m_Act_Difficulty < 0 )
		{
			CScriptTriggerArgs	Args ( 0, 0, pWeapon );
			if (g_Cfg.IsSkillRanged(skill))
			{
				// Get uid of the current arrow.
				if (pAmmo)
				{
					Args.m_VarsLocal.SetNum("Arrow", pAmmo->GetUID());
				}
			}
			if ( OnTrigger( CTRIG_HitMiss, pCharTarg, &Args ) == TRIGRET_RET_TRUE )
				return( WAR_SWING_EQUIPPING );

			// If arrow is handled by script, do nothing with it further!
			if (Args.m_VarsLocal.GetKeyNum("ArrowHandled") != 0)
				pAmmo = NULL;
		}
		if ( pWeapon )
			pTagStorage = pWeapon->GetKey("OVERRIDE.SOUND_MISS", true);
		if ( pTagStorage )
		{
			if ( pTagStorage->GetValNum() )
			{
				iSnd = static_cast<SOUND_TYPE>(pTagStorage->GetValNum());
			}
		}
	}

	if ( m_Act_Difficulty < 0 )		// if not changed within trigger
	{
		// We missed. (miss noise)
		if ( g_Cfg.IsSkillRanged(skill) )
		{
			// 0x223 = bolt miss or dart ?
			// do some thing with the arrow.
			if ( pTagStorage != NULL )
				Sound( iSnd );
			else
				Sound( Calc_GetRandVal(2) ? 0x233 : 0x238 );

			// Sometime arrows should be lost/broken when we miss

			if ( pAmmo && Calc_GetRandVal(5))
			{
				// int pAmmo->OnTakeDamage( 2, pSrc, DAMAGE_HIT_BLUNT )
				pAmmo->MoveToCheck( pCharTarg->GetTopPoint(), this );
			}
			return( WAR_SWING_EQUIPPING );	// Made our full swing. (tho we missed)
		}

		static const SOUND_TYPE sm_Snd_Miss[] =
		{
			0x238,	// = swish01
			0x239,	// = swish02
			0x23a	// = swish03
		};
		if ( pTagStorage != NULL )
			Sound( iSnd );
		else
			Sound( sm_Snd_Miss[ Calc_GetRandVal( COUNTOF( sm_Snd_Miss )) ] );

		if ( IsPriv(PRIV_DETAIL))
		{
			SysMessagef( g_Cfg.GetDefaultMsg( DEFMSG_COMBAT_MISSS ), static_cast<LPCTSTR>(pCharTarg->GetName()));
		}
		if ( pCharTarg->IsPriv(PRIV_DETAIL))
		{
			pCharTarg->SysMessagef( g_Cfg.GetDefaultMsg( DEFMSG_COMBAT_MISSO ), static_cast<LPCTSTR>(GetName()));
		}

		return( WAR_SWING_EQUIPPING );	// Made our full swing. (tho we missed)
	}

	// BAD BAD Healing fix.. Cant think of something else -- Radiant
	if (pCharTarg->m_Act_SkillCurrent == SKILL_HEALING)
	{
		pCharTarg->SysMessageDefault( DEFMSG_HEALING_INTERRUPT );
		pCharTarg->Skill_Cleanup();
	}

	// We hit...
	// Calculate base damage.
	int	iDmg = Fight_CalcDamage( pWeapon, skill );

	//if ( iDmg ) // use OnTakeDamage below instead.
	//	pCharTarg->OnHarmedBy( this, iDmg );

	CScriptTriggerArgs	Args( iDmg, 0, pWeapon );

	if ( g_Cfg.IsSkillRanged(skill) )
	{
		// Get uid of the current arrow.
		if ( pAmmo )
		{			
			Args.m_VarsLocal.SetNum("Arrow", pAmmo->GetUID());
		}
	}

	if ( IsTrigUsed(TRIGGER_SKILLSUCCESS) )
	{
		if ( Skill_OnCharTrigger( skill, CTRIG_SkillSuccess ) == TRIGRET_RET_TRUE )
		{
			Skill_Cleanup();
			return( WAR_SWING_EQUIPPING );	// ok, so no hit - skill failed. Pah!
		}
	}
	if ( IsTrigUsed(TRIGGER_SUCCESS) )
	{
		if ( Skill_OnTrigger( skill, SKTRIG_SUCCESS ) == TRIGRET_RET_TRUE )
		{
			Skill_Cleanup();
			return( WAR_SWING_EQUIPPING );	// ok, so no hit - skill failed. Pah!
		}
	}

	if ( IsTrigUsed(TRIGGER_HIT) )
	{
		if ( OnTrigger( CTRIG_Hit, pCharTarg, &Args ) == TRIGRET_RET_TRUE )
			return( WAR_SWING_EQUIPPING );

		// If arrow is handled by script, do nothing with it further!
		if (Args.m_VarsLocal.GetKeyNum("ArrowHandled") != 0)
			pAmmo = NULL;
	}
#ifdef _ALPHASPHERE
	Memory_AddObjTypes(pCharTarg,MEMORY_WAR_TARG);
#endif
	iDmg	= static_cast<int>(Args.m_iN1);

	if ( g_Cfg.IsSkillRanged(skill) )
	{
		// There's a chance that the arrow will stick in the target
		if ( pAmmo && !Calc_GetRandVal(2))
		{
			// int pAmmo->OnTakeDamage( 2+iDmg, pCharTarg, DAMAGE_HIT_BLUNT )

			pCharTarg->ItemBounce( pAmmo );
		}
	}

	// Raise skill
	if ( ! (pCharTarg->m_pArea->IsFlag(REGION_FLAG_NO_PVP) && m_pPlayer && pCharTarg->m_pPlayer))
		Skill_UseQuick( SKILL_TACTICS, pCharTarg->Skill_GetBase(SKILL_TACTICS)/10 );

	// Hit noise. based on weapon type.
	SoundChar( CRESND_HIT );

	if ( pWeapon != NULL )
	{
		// poison weapon ?
		if ( pWeapon->m_itWeapon.m_poison_skill &&
			Calc_GetRandVal( 100 ) < pWeapon->m_itWeapon.m_poison_skill )
		{
			// Poison delivered.
			BYTE iPoisonDeliver = Calc_GetRandVal(pWeapon->m_itWeapon.m_poison_skill);

			pCharTarg->SetPoison( 10 * iPoisonDeliver, iPoisonDeliver / 5, this );

			// Diminish the poison on the weapon.
			pWeapon->m_itWeapon.m_poison_skill -= iPoisonDeliver / 2;
			pWeapon->UpdatePropertyFlag(AUTOTOOLTIP_FLAG_POISON);
		}

		// damage the weapon ?
		pWeapon->OnTakeDamage( iDmg/4, pCharTarg );
	}
	else
	{
		// Base type attack for our body. claws/etc
		// intrinsic attacks ?
		// Poisonous bite/sting ?
		if ( m_pNPC &&
			m_pNPC->m_Brain == NPCBRAIN_MONSTER &&
			Skill_GetBase(SKILL_POISONING) > 300 &&
			Calc_GetRandVal( 1000 ) < Skill_GetBase(SKILL_POISONING))
		{
			// Poison delivered.
			int iSkill = Skill_GetAdjusted( SKILL_POISONING );
			pCharTarg->SetPoison( Calc_GetRandVal(iSkill), Calc_GetRandVal(iSkill/50), this );
		}
	}

	// Took my swing. Do Damage !
#ifdef _ALPHASPHERE
	if (pWeapon != NULL )
	{
		CScriptTriggerArgs weaponArgs( iDmg, (int) iTyp );
		if ( pWeapon->OnTrigger( ITRIG_DAMAGEGIVEN, this, &weaponArgs ) == TRIGRET_RET_TRUE )
			iDmg = 0;
		else {
			iDmg = weaponArgs.m_iN1;
			iTyp = weaponArgs.m_iN2;
		}
	}
#endif
	iDmg = pCharTarg->OnTakeDamage( iDmg, this, iTyp );
	if ( iDmg > 0 )
	{
		// Is we do no damage we get no experience!
		Skill_Experience( skill, m_Act_Difficulty );	// Get experience for it.
	}

	return( WAR_SWING_EQUIPPING );	// Made our full swing.
}
