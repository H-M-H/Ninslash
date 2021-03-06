#include <game/generated/protocol.h>
#include <game/server/gamecontext.h>
#include "character.h"
#include "building.h"
#include "deathray.h"

CDeathray::CDeathray(CGameWorld *pGameWorld, vec2 Pos)
: CBuilding(pGameWorld, Pos, BUILDING_LAZER, TEAM_NEUTRAL)
{
	m_ProximityRadius = TurretPhysSize;
	m_Life = 100;
	m_AttackTick = 0;
	m_Loading = false;
	m_Height = 0;
}



void CDeathray::Tick()
{
	if (m_Loading)
	{
		if (m_AttackTick + Server()->TickSpeed()*3.4f < GameServer()->Server()->Tick())
		{
			m_Loading = false;
			m_AttackTick = GameServer()->Server()->Tick();
			m_Height = GameServer()->CreateDeathray(m_Pos+vec2(0, 8));
		}
	}
	else
	{
		if (m_AttackTick + Server()->TickSpeed()*3 < GameServer()->Server()->Tick())
		{
			m_Loading = true;
			GameServer()->CreateEffect(FX_LAZERLOAD, m_Pos+vec2(0, 32));
		}
	}
	
	
	// death ray on characters & buildings
	if (m_AttackTick > GameServer()->Server()->Tick() - Server()->TickSpeed()*0.25f)
	{
		vec2 At;
		CCharacter *pHit = GameServer()->m_World.IntersectCharacter(m_Pos, m_Pos+vec2(0, m_Height), 4.0f, At);
		
		if(pHit)
			pHit->Deathray();
		
		CBuilding *pBHit = GameServer()->m_World.IntersectBuilding(m_Pos+vec2(0, 48), m_Pos+vec2(0, m_Height), 4.0f, At, -666);
		
		if(pBHit && pBHit != this)
			pBHit->TakeDamage(1000, -1, -1);
	}
}