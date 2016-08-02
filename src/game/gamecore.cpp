

#include "gamecore.h"

const char *CTuningParams::m_apNames[] =
{
	#define MACRO_TUNING_PARAM(Name,ScriptName,Value) #ScriptName,
	#include "tuning.h"
	#undef MACRO_TUNING_PARAM
};


bool CTuningParams::Set(int Index, float Value)
{
	if(Index < 0 || Index >= Num())
		return false;
	((CTuneParam *)this)[Index] = Value;
	return true;
}

bool CTuningParams::Get(int Index, float *pValue)
{
	if(Index < 0 || Index >= Num())
		return false;
	*pValue = (float)((CTuneParam *)this)[Index];
	return true;
}

bool CTuningParams::Set(const char *pName, float Value)
{
	for(int i = 0; i < Num(); i++)
		if(str_comp_nocase(pName, m_apNames[i]) == 0)
			return Set(i, Value);
	return false;
}

bool CTuningParams::Get(const char *pName, float *pValue)
{
	for(int i = 0; i < Num(); i++)
		if(str_comp_nocase(pName, m_apNames[i]) == 0)
			return Get(i, pValue);

	return false;
}

float HermiteBasis1(float v)
{
	return 2*v*v*v - 3*v*v+1;
}

float VelocityRamp(float Value, float Start, float Range, float Curvature)
{
	if(Value < Start)
		return 1.0f;
	return 1.0f/powf(Curvature, (Value-Start)/Range);
}

void CCharacterCore::Init(CWorldCore *pWorld, CCollision *pCollision)
{
	m_pWorld = pWorld;
	m_pCollision = pCollision;
}

void CCharacterCore::Reset()
{
	m_DamageTick = 0;
	m_Pos = vec2(0,0);
	m_Vel = vec2(0,0);
	m_HookPos = vec2(0,0);
	m_HookDir = vec2(0,0);
	m_HookTick = 0;
	m_HookState = HOOK_IDLE;
	m_HookedPlayer = -1;
	m_Jumped = 0;
	m_Sliding = 0;
	m_Anim = 0;
	m_LockDirection = 0;
	m_Jetpack = 0;
	m_JetpackPower = 100;
	m_Wallrun = 0;
	m_Roll = 0;
	m_Status = 0;
	m_Status |= 1 << STATUS_SPAWNING;
	m_TriggeredEvents = 0;
}


bool CCharacterCore::IsGrounded() {
	float PhysSize = 28.0f;
	
	for(int i = -PhysSize/2; i <= PhysSize/2; i++) {
		if(m_pCollision->CheckPoint(m_Pos.x+i, m_Pos.y+PhysSize/2+5)) {
			return true;
		}
	}
	
	return false;
}


int CCharacterCore::SlopeState() {
	float PhysSize = 28.0f;
	
	int tmp = 0;
	int height_left = 0;
	for(int x = -1; x <= -1; x++)
		for(int y = 0; y <= 4; y++)
			if ( (tmp = m_pCollision->CheckPoint(m_Pos.x-PhysSize/2+x, m_Pos.y+PhysSize/2+y)) > height_left) {
				height_left = tmp;
			}
		
	
	int height_right = 0;
	for(int x = 1; x <= 1; x++)
		for(int y = 0; y <= 4; y++)
			if ( (tmp = m_pCollision->CheckPoint(m_Pos.x+PhysSize/2+x, m_Pos.y+PhysSize/2+y)) > height_right) {
				height_right = tmp;
			}
	

// 	if ( (tmp = m_pCollision->CheckPoint(m_Pos.x-PhysSize/2-1, m_Pos.y+PhysSize/2+1)) > height_left)
// 		height_left = tmp;
// 	if ( (tmp = m_pCollision->CheckPoint(m_Pos.x-PhysSize/2, m_Pos.y+PhysSize/2+1)) > height_left)
// 		height_left = tmp;
// 	
// 	int height_right = max(m_pCollision->CheckPoint(m_Pos.x+PhysSize/2+1, m_Pos.y+PhysSize/2), m_pCollision->CheckPoint(m_Pos.x+PhysSize/2, m_Pos.y+PhysSize/2));
// 	if ( (tmp = m_pCollision->CheckPoint(m_Pos.x+PhysSize/2, m_Pos.y+PhysSize/2)) > height_right)
// 		height_right = tmp;
// 	if ( (tmp = m_pCollision->CheckPoint(m_Pos.x+PhysSize/2+1, m_Pos.y+PhysSize/2+1)) > height_right)
// 		height_right = tmp;
// 	if ( (tmp = m_pCollision->CheckPoint(m_Pos.x+PhysSize/2, m_Pos.y+PhysSize/2+1)) > height_right)
// 		height_right = tmp;
	//left hand side
	/*for(int j = 0; j <= 28; j++) {
		if(m_pCollision->CheckPoint(m_Pos.x-PhysSize/2-1, m_Pos.y+PhysSize/2+j)) {
			height_left = j;
			break;
		}
	}
	
	for(int j = 0; j <=28; j++) {
		if(m_pCollision->CheckPoint(m_Pos.x+PhysSize/2+1, m_Pos.y+PhysSize/2+j)) {
			height_right = j;
			break;
		}
	}*/
	//return 1;
	//std::cerr << "HEIGHTS: " << height_left << " - " << height_right << std::endl;
	if(height_left == CCollision::SS_COL_RL) {
		//std::cerr << "RET 1" << std::endl;
		return 1;
	}
	else if (height_right == CCollision::SS_COL_RR) {
		//std::cerr << "RET -1" << std::endl;
		return -1;
	}
	//std::cerr << "RET 0" << std::endl;
	return 0;
}



void CCharacterCore::Tick(bool UseInput)
{
	m_MonsterDamage = false;
	m_HandJetpack = false;
	
	int s = m_Status;
	if (s & (1<<STATUS_DEATHRAY))
		return;
	
	if (Status(STATUS_SPAWNING))
		return;
	
	float PhysSize = 28.0f;
	m_TriggeredEvents = 0;

	// get ground state

	bool Grounded = IsGrounded();
	int slope = SlopeState();
	
	/*
	bool Grounded = false;
	if(m_pCollision->CheckPoint(m_Pos.x+PhysSize/2, m_Pos.y+PhysSize/2+5))
		Grounded = true;
	if(m_pCollision->CheckPoint(m_Pos.x-PhysSize/2, m_Pos.y+PhysSize/2+5))
		Grounded = true;
	*/

	vec2 TargetDirection = normalize(vec2(m_Input.m_TargetX, m_Input.m_TargetY));

	m_Vel.y += m_pWorld->m_Tuning.m_Gravity;

	float MaxSpeed = Grounded ? m_pWorld->m_Tuning.m_GroundControlSpeed : m_pWorld->m_Tuning.m_AirControlSpeed;
	float Accel = Grounded ? m_pWorld->m_Tuning.m_GroundControlAccel : m_pWorld->m_Tuning.m_AirControlAccel;
	float Friction = Grounded ? m_pWorld->m_Tuning.m_GroundFriction : m_pWorld->m_Tuning.m_AirFriction;
	float SlideFriction = m_pWorld->m_Tuning.m_SlideFriction;

	float SlideSlopeAcceleration = m_pWorld->m_Tuning.m_SlideSlopeAcceleration;
	float SlopeDeceleration = m_pWorld->m_Tuning.m_SlopeDeceleration;
	float SlopeAscendingControlSpeed = m_pWorld->m_Tuning.m_SlopeAscendingControlSpeed*invsqrt2;
	float SlopeDescendingControlSpeed = m_pWorld->m_Tuning.m_SlopeDescendingControlSpeed*invsqrt2;
	float SlideControlSpeed = m_pWorld->m_Tuning.m_SlideControlSpeed*invsqrt2;
	
	float SlideActivationSpeed = m_pWorld->m_Tuning.m_SlideActivationSpeed;

	float JumpPower = m_pWorld->m_Tuning.m_GroundJumpImpulse;
	float AirJumpPower = m_pWorld->m_Tuning.m_AirJumpImpulse;
	float WallrunPower = m_pWorld->m_Tuning.m_WallrunImpulse;
	
	s = m_Status;
	if (s & (1<<STATUS_RAGE))
	{
		Friction /= 1.4f;
		MaxSpeed *= 1.4f;
		Accel *= 1.4f;
		JumpPower *= 1.3f;
		AirJumpPower *= 1.3f;
		WallrunPower *= 1.3f;
	}
		
	m_Anim = 0;
	
	bool LoadJetpack = false;
	
	// fill jetpack
	if (Grounded)
	{
		LoadJetpack = true;
	}
	
	
	// handle input
	if(UseInput)
	{
		m_Direction = m_Input.m_Direction;
		
		if (m_LockDirection > 0)
		{
			m_LockDirection--;
			m_Direction = 1;
		}
		if (m_LockDirection < 0)
		{
			m_LockDirection++;
			m_Direction = -1;
		}

		// wall climbing
		if (!Grounded && m_Jetpack == 0)
		{
			// falling down
			if (m_Vel.y > 0)
			{
				if (m_Wallrun > 10 && m_Wallrun < 25)
					m_Wallrun++;
				else if (m_Wallrun < -10 && m_Wallrun > -25)
					m_Wallrun--;
				else
					m_Wallrun = 0;
				
				if (m_Direction < 0 && m_pCollision->CheckPoint(m_Pos.x-PhysSize, m_Pos.y+PhysSize/2) &&
										m_pCollision->CheckPoint(m_Pos.x-PhysSize, m_Pos.y-PhysSize/2))
				{
					m_Vel.y *= 0.75f;
					m_Anim = 1;
					
					LoadJetpack = true;
				}
					
				if (m_Direction > 0 && m_pCollision->CheckPoint(m_Pos.x+PhysSize, m_Pos.y+PhysSize/2) &&
										m_pCollision->CheckPoint(m_Pos.x+PhysSize, m_Pos.y-PhysSize/2))
				{
					m_Vel.y *= 0.75f;
					m_Anim = -1;
					
					LoadJetpack = true;
				}
				
				// wall jump
				if(m_Input.m_Jump && !(m_Jumped&1))
				{
					if (m_pCollision->CheckPoint(m_Pos.x-(PhysSize+6), m_Pos.y+PhysSize/2) &&
						m_pCollision->CheckPoint(m_Pos.x-(PhysSize+6), m_Pos.y-PhysSize/2))
					{
						//m_TriggeredEvents |= COREEVENT_AIR_JUMP;
						m_Vel.y = -AirJumpPower;
						m_Vel.x = 10.0f;
						m_Jumped |= 1;
						m_LockDirection = 4;
						m_Wallrun = 11;
					}
						
					if (m_pCollision->CheckPoint(m_Pos.x+(PhysSize+6), m_Pos.y+PhysSize/2) &&
						m_pCollision->CheckPoint(m_Pos.x+(PhysSize+6), m_Pos.y-PhysSize/2))
					{
						//m_TriggeredEvents |= COREEVENT_AIR_JUMP;
						m_Vel.y = -AirJumpPower;
						m_Vel.x = -10.0f;
						m_Jumped |= 1;
						m_LockDirection = -4;
						m_Wallrun = -11;
					}
				}
			}
			
			// going up
			else
			{
				// wallrun
				if (m_pCollision->CheckPoint(m_Pos.x-(PhysSize+6), m_Pos.y+PhysSize/2) &&
					m_pCollision->CheckPoint(m_Pos.x-(PhysSize+6), m_Pos.y-PhysSize/2))
				{
					// run up
					if (++m_Wallrun > 5 && m_Wallrun < 11)
					{
						m_Wallrun = 1;
						m_Vel.y = -WallrunPower;
					}
					else
						if (m_Wallrun < -15)
							m_Wallrun = 3;
					
					LoadJetpack = true;
					
					// wall jump
					if(m_Input.m_Jump && !(m_Jumped&1))
					{
						//m_TriggeredEvents |= COREEVENT_AIR_JUMP;
						m_Vel.y = -(AirJumpPower+3.0f);
						m_Vel.x = 10.0f+2.0f;
						m_Jumped |= 1;
						m_LockDirection = 4;
						m_Wallrun = 11;
					}
				}
				else
				if (m_pCollision->CheckPoint(m_Pos.x+(PhysSize+6), m_Pos.y+PhysSize/2) &&
					m_pCollision->CheckPoint(m_Pos.x+(PhysSize+6), m_Pos.y-PhysSize/2))
				{
					// run up
					if (--m_Wallrun < -5 && m_Wallrun > -11)
					{
						m_Wallrun = -1;
						m_Vel.y = -WallrunPower;
					}
					else
						if (m_Wallrun > 15)
							m_Wallrun = -3;

					LoadJetpack = true;
						
					// wall jump
					if(m_Input.m_Jump && !(m_Jumped&1))
					{
						//m_TriggeredEvents |= COREEVENT_AIR_JUMP;
						m_Vel.y = -(AirJumpPower+3.0f);
						m_Vel.x = -(10.0f+2.0f);
						m_Jumped |= 1;
						m_LockDirection = -4;
						m_Wallrun = -11;
					}
				}
				else
				{
					if (m_Wallrun > 10 && m_Wallrun < 25)
						m_Wallrun++;
					else if (m_Wallrun < -10 && m_Wallrun > -25)
						m_Wallrun--;
					else
						m_Wallrun = 0;
				}
			}
		}
		
		
		// setup angle
		float a = 0;
		if(m_Input.m_TargetX == 0)
			a = atanf((float)m_Input.m_TargetY);
		else
			a = atanf((float)m_Input.m_TargetY/(float)m_Input.m_TargetX);

		if(m_Input.m_TargetX < 0)
			a = a+pi;

		m_Angle = (int)(a*256.0f);

		// handle jump
		if(m_Input.m_Jump)
		{
			// jetpack physics
			if (m_Jetpack == 1 && m_JetpackPower > 0 && m_Wallrun == 0)
			{
				//if (--m_JetpackPower <= 0)
				//	m_Jetpack = 0;
				
				if (m_Vel.y > -10.0f)
					m_Vel.y -= 1.2f;
				
				if (m_Direction == 1 && m_Vel.x < 14.0f)
					m_Vel.x += 0.5f;
				
				if (m_Direction == -1 && m_Vel.x > -14.0f)
					m_Vel.x -= 0.5f;
				
				m_JetpackPower -= 2;
			}
			
			
			if(!(m_Jumped&1)) // && m_LockDirection == 0)
			{
				if(Grounded)
				{
					m_TriggeredEvents |= COREEVENT_GROUND_JUMP;
					if(slope == 0) 
						m_Vel.y = -JumpPower;
					else {
						m_Vel.y = -JumpPower*invsqrt2;
					}
					m_Jumped |= 1;
				}
				// launch jetpack
				else if(!(m_Jumped&2) && m_JetpackPower > 0)
				{
					m_Jetpack = 1;
				}			
				
				/*
				// air jump
				else if(!(m_Jumped&2))
				{
					m_TriggeredEvents |= COREEVENT_AIR_JUMP;
					m_Vel.y = -AirJumpPower;
					m_Jumped |= 3;
				}
				*/
			}
		}
		else
		{
			m_Jumped &= ~1;
			m_Jetpack = 0;
		}

		
		if(m_Input.m_Hook && m_JetpackPower > 0)
		{
			if ((TargetDirection.x > 0 && m_Vel.x < 14.0f) || (TargetDirection.x < 0 && m_Vel.x > -14.0f))
				m_Vel.x += TargetDirection.x*1.2f;
				
			if ((TargetDirection.y > 0 && m_Vel.y < 14.0f) || (TargetDirection.y < 0 && m_Vel.y > -14.0f))
				m_Vel.y += TargetDirection.y*1.2f;
				
			m_JetpackPower -= 2;
			m_HandJetpack = true;
		}
		
		// handle hook & disable hook for a while
		if(m_Input.m_Hook && false)
		{
			if(m_HookState == HOOK_IDLE)
			{
				m_HookState = HOOK_FLYING;
				m_HookPos = m_Pos+TargetDirection*PhysSize*1.5f;
				m_HookDir = TargetDirection;
				m_HookedPlayer = -1;
				m_HookTick = 0;
				m_TriggeredEvents |= COREEVENT_HOOK_LAUNCH;
			}
		}
		else
		{
			m_HookedPlayer = -1;
			m_HookState = HOOK_IDLE;
			m_HookPos = m_Pos;
		}
	}

	m_Sliding = false;
	
	if(slope != 0)
		m_Sliding = true;
	
	if( (m_Vel.x > SlideActivationSpeed && slope == 1) || (m_Vel.x < -SlideActivationSpeed && slope == -1)) {
		m_Sliding = true;
	}
	

	if(slope != 0 && m_Direction == slope && !m_Sliding)
		MaxSpeed = SlopeDescendingControlSpeed;
	else if(slope != 0 && m_Direction == -slope && !m_Sliding) {
		MaxSpeed = SlopeAscendingControlSpeed;
		float diff = SlopeDeceleration*fabs(m_Vel.x - MaxSpeed);
		/*if(m_Vel.x > MaxSpeed)
			m_Vel.x = SaturatedAdd(-MaxSpeed, MaxSpeed, m_Vel.x, -SlopeDeceleration);
		if(m_Vel.x < -MaxSpeed)
			m_Vel.x = SaturatedAdd(-MaxSpeed, MaxSpeed, m_Vel.x, SlopeDeceleration);*/
		if(m_Vel.x > MaxSpeed)
			m_Vel.x -= diff;
		if(m_Vel.x < -MaxSpeed)
			m_Vel.x += diff;
	}
	
	// add the speed modification according to players wanted direction
	if(m_Direction < 0) // && (!m_Sliding || !Grounded))
	{
		if (slope > 0)
			m_Vel.x = SaturatedAdd(-MaxSpeed, MaxSpeed, m_Vel.x, -Accel*0.5f);
		else
			m_Vel.x = SaturatedAdd(-MaxSpeed, MaxSpeed, m_Vel.x, -Accel);
	}
	if(m_Direction > 0) // && (!m_Sliding || !Grounded))
	{
		if (slope < 0)
			m_Vel.x = SaturatedAdd(-MaxSpeed, MaxSpeed, m_Vel.x, Accel*0.5f);
		else
			m_Vel.x = SaturatedAdd(-MaxSpeed, MaxSpeed, m_Vel.x, Accel);
	}
	
	if(m_Sliding && slope != 0) {
		//std::cerr << "Fric1..." << m_Pos.x << std::endl;
		m_Vel.x = SaturatedAdd(-SlideControlSpeed, SlideControlSpeed, m_Vel.x, slope*SlideSlopeAcceleration);
		//m_Vel.y = slope*SaturatedAdd(-SlideControlSpeed, SlideControlSpeed, m_Vel.y, slope*SlideSlopeAcceleration);
	}
	else if(m_Sliding && Grounded) {
		//std::cerr << "Fric2..." << m_Pos.x << ", " << SlideFriction << std::endl;
		m_Vel.x *= SlideFriction;
	}
	else if(m_Direction == 0) {
		//std::cerr << "Fric3..." << m_Pos.x << std::endl;
		if(slope != 0 && !m_Jumped) {
			m_Vel.x *= Friction;// /invsqrt2;
			m_Vel.y *= Friction;// /invsqrt2;
		} else {
			m_Vel.x *= Friction;
		}
	}
	
	
	/*
	// add the speed modification according to players wanted direction
	if(m_Direction < 0)
		m_Vel.x = SaturatedAdd(-MaxSpeed, MaxSpeed, m_Vel.x, -Accel);
	if(m_Direction > 0)
		m_Vel.x = SaturatedAdd(-MaxSpeed, MaxSpeed, m_Vel.x, Accel);
	if(m_Direction == 0)
		m_Vel.x *= Friction;
	*/

	
	// handle jumping
	// 1 bit = to keep track if a jump has been made on this input
	// 2 bit = to keep track if a air-jump has been made
	if(Grounded)
		m_Jumped &= ~2;
	
	if (LoadJetpack)
	{
		m_JetpackPower += 2;
		
		if (m_JetpackPower > 100)
			m_JetpackPower = 100;
	}
	
	
	if (m_Roll > 0)
		m_Roll++;
	
	
	if (m_Roll > 0)
	{
		if (m_Vel.x < -2.0f)
		{
			if (m_Roll < 16)
				m_Anim = -2;
			else
				m_Roll = -0;
		}
		else if (m_Vel.x > 2.0f)
		{
			if (m_Roll < 16)
				m_Anim = 2;
			else
				m_Roll = -0;
		}
		else
		{
			m_Roll = 0;
		}
	}

	// do hook
	if(m_HookState == HOOK_IDLE)
	{
		m_HookedPlayer = -1;
		m_HookState = HOOK_IDLE;
		m_HookPos = m_Pos;
	}
	else if(m_HookState >= HOOK_RETRACT_START && m_HookState < HOOK_RETRACT_END)
	{
		m_HookState++;
	}
	else if(m_HookState == HOOK_RETRACT_END)
	{
		m_HookState = HOOK_RETRACTED;
		m_TriggeredEvents |= COREEVENT_HOOK_RETRACT;
		m_HookState = HOOK_RETRACTED;
	}
	else if(m_HookState == HOOK_FLYING)
	{
		vec2 NewPos = m_HookPos+m_HookDir*m_pWorld->m_Tuning.m_HookFireSpeed;
		if(distance(m_Pos, NewPos) > m_pWorld->m_Tuning.m_HookLength)
		{
			m_HookState = HOOK_RETRACT_START;
			NewPos = m_Pos + normalize(NewPos-m_Pos) * m_pWorld->m_Tuning.m_HookLength;
		}

		// make sure that the hook doesn't go though the ground
		bool GoingToHitGround = false;
		bool GoingToRetract = false;
		int Hit = m_pCollision->IntersectLine(m_HookPos, NewPos, &NewPos, 0);
		if(Hit)
		{
			if(Hit&CCollision::COLFLAG_NOHOOK)
				GoingToRetract = true;
			else
				GoingToHitGround = true;
		}

		// Check against other players first
		if(m_pWorld && m_pWorld->m_Tuning.m_PlayerHooking)
		{
			float Distance = 0.0f;
			for(int i = 0; i < MAX_CLIENTS; i++)
			{
				CCharacterCore *pCharCore = m_pWorld->m_apCharacters[i];
				if(!pCharCore || pCharCore == this)
					continue;

				vec2 ClosestPoint = closest_point_on_line(m_HookPos, NewPos, pCharCore->m_Pos);
				if(distance(pCharCore->m_Pos, ClosestPoint) < PhysSize+2.0f)
				{
					if (m_HookedPlayer == -1 || distance(m_HookPos, pCharCore->m_Pos) < Distance)
					{
						m_TriggeredEvents |= COREEVENT_HOOK_ATTACH_PLAYER;
						m_HookState = HOOK_GRABBED;
						m_HookedPlayer = i;
						Distance = distance(m_HookPos, pCharCore->m_Pos);
					}
				}
			}
		}

		if(m_HookState == HOOK_FLYING)
		{
			// check against ground
			if(GoingToHitGround)
			{
				m_TriggeredEvents |= COREEVENT_HOOK_ATTACH_GROUND;
				m_HookState = HOOK_GRABBED;
			}
			else if(GoingToRetract)
			{
				m_TriggeredEvents |= COREEVENT_HOOK_HIT_NOHOOK;
				m_HookState = HOOK_RETRACT_START;
			}

			m_HookPos = NewPos;
		}
	}

	if(m_HookState == HOOK_GRABBED)
	{
		if(m_HookedPlayer != -1)
		{
			CCharacterCore *pCharCore = m_pWorld->m_apCharacters[m_HookedPlayer];
			if(pCharCore)
				m_HookPos = pCharCore->m_Pos;
			else
			{
				// release hook
				m_HookedPlayer = -1;
				m_HookState = HOOK_RETRACTED;
				m_HookPos = m_Pos;
			}

			// keep players hooked for a max of 1.5sec
			//if(Server()->Tick() > hook_tick+(Server()->TickSpeed()*3)/2)
				//release_hooked();
		}

		// don't do this hook rutine when we are hook to a player
		if(m_HookedPlayer == -1 && distance(m_HookPos, m_Pos) > 46.0f)
		{
			vec2 HookVel = normalize(m_HookPos-m_Pos)*m_pWorld->m_Tuning.m_HookDragAccel;
			// the hook as more power to drag you up then down.
			// this makes it easier to get on top of an platform
			if(HookVel.y > 0)
				HookVel.y *= 0.3f;

			// the hook will boost it's power if the player wants to move
			// in that direction. otherwise it will dampen everything abit
			if((HookVel.x < 0 && m_Direction < 0) || (HookVel.x > 0 && m_Direction > 0))
				HookVel.x *= 0.95f;
			else
				HookVel.x *= 0.75f;

			vec2 NewVel = m_Vel+HookVel;

			// check if we are under the legal limit for the hook
			if(length(NewVel) < m_pWorld->m_Tuning.m_HookDragSpeed || length(NewVel) < length(m_Vel))
				m_Vel = NewVel; // no problem. apply

		}

		// release hook (max hook time is 1.25
		m_HookTick++;
		if(m_HookedPlayer != -1 && (m_HookTick > SERVER_TICK_SPEED+SERVER_TICK_SPEED/5 || !m_pWorld->m_apCharacters[m_HookedPlayer]))
		{
			m_HookedPlayer = -1;
			m_HookState = HOOK_RETRACTED;
			m_HookPos = m_Pos;
		}
	}

	if(m_pWorld)
	{
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			CCharacterCore *pCharCore = m_pWorld->m_apCharacters[i];
			if(!pCharCore)
				continue;

			//player *p = (player*)ent;
			if(pCharCore == this  || pCharCore->Status(STATUS_SPAWNING)) // || !(p->flags&FLAG_ALIVE)
				continue; // make sure that we don't nudge our self

			// handle player <-> player collision
			float Distance = distance(m_Pos, pCharCore->m_Pos);
			vec2 Dir = normalize(m_Pos - pCharCore->m_Pos);
			if(m_pWorld->m_Tuning.m_PlayerCollision && m_Roll == 0 && pCharCore->m_Roll == 0 && Distance < PhysSize*1.25f && Distance > 0.0f)
			{
				float a = (PhysSize*1.45f - Distance);
				float Velocity = 0.5f;

				// make sure that we don't add excess force by checking the
				// direction against the current velocity. if not zero.
				if (length(m_Vel) > 0.0001)
					Velocity = 1-(dot(normalize(m_Vel), Dir)+1)/2;

				m_Vel += Dir*a*(Velocity*0.75f);
				m_Vel *= 0.85f;
			}

			// handle hook influence
			/*
			if(m_HookedPlayer == i && m_pWorld->m_Tuning.m_PlayerHooking)
			{
				if(Distance > PhysSize*1.50f) // TODO: fix tweakable variable
				{
					float Accel = m_pWorld->m_Tuning.m_HookDragAccel * (Distance/m_pWorld->m_Tuning.m_HookLength);
					float DragSpeed = m_pWorld->m_Tuning.m_HookDragSpeed;

					// add force to the hooked player
					pCharCore->m_Vel.x = SaturatedAdd(-DragSpeed, DragSpeed, pCharCore->m_Vel.x, Accel*Dir.x*1.5f);
					pCharCore->m_Vel.y = SaturatedAdd(-DragSpeed, DragSpeed, pCharCore->m_Vel.y, Accel*Dir.y*1.5f);

					// add a little bit force to the guy who has the grip
					m_Vel.x = SaturatedAdd(-DragSpeed, DragSpeed, m_Vel.x, -Accel*Dir.x*0.25f);
					m_Vel.y = SaturatedAdd(-DragSpeed, DragSpeed, m_Vel.y, -Accel*Dir.y*0.25f);
				}
			}
			*/
		}
		
		// monster collision
		for(int i = 0; i < MAX_MONSTERS; i++)
		{
			vec2 MonsterPos = m_pWorld->m_aMonsterPos[i];
				
			if(MonsterPos.x == 0)
				continue;

			// handle player <-> monster collision
			float Distance = distance(m_Pos, MonsterPos);
			vec2 Dir = normalize(m_Pos - MonsterPos);
			if(Distance < PhysSize*1.65f && Distance > 0.0f)
			{
				//m_Status |= 1 << STATUS_ELECTRIC;
				m_Vel = Dir*12.0f; // + vec2(0, -4);
				m_MonsterDamage = true;
			}
		}
	}

	// clamp the velocity to something sane
	if(length(m_Vel) > 6000)
		m_Vel = normalize(m_Vel) * 6000;
	
	// electric damage effect
	s = m_Status;
	if (s & (1<<STATUS_ELECTRIC))
		m_Vel.x *= 0.85f;
}


void CCharacterCore::Roll()
{
	float PhysSize = 28.0f;
	
	if (m_Vel.x < -2.0f && !m_pCollision->CheckPoint(m_Pos.x-(PhysSize+32), m_Pos.y+PhysSize/2))
	{
		m_Roll++;
		m_LockDirection = -8;
	}
	else if (m_Vel.x > 2.0f && !m_pCollision->CheckPoint(m_Pos.x+(PhysSize+32), m_Pos.y+PhysSize/2))
	{
		m_Roll++;
		m_LockDirection = 8;
	}
}
	
	
void CCharacterCore::Move()
{
	int s = m_Status;
	if (s & (1<<STATUS_DEATHRAY))
		return;
	
	if (Status(STATUS_SPAWNING))
		return;
	
	float RampValue = VelocityRamp(length(m_Vel)*50, m_pWorld->m_Tuning.m_VelrampStart, m_pWorld->m_Tuning.m_VelrampRange, m_pWorld->m_Tuning.m_VelrampCurvature);

	m_Vel.x = m_Vel.x*RampValue;

	float VelY = m_Vel.y;
	
	vec2 NewPos = m_Pos;
	NewPos.y -= 18;
	m_pCollision->MoveBox(&NewPos, &m_Vel, vec2(28.0f, 64.0f), 0, !m_Sliding);
	NewPos.y += 18;
	
	if (VelY > 20.0f && abs(m_Vel.y) < 2.0f)
		Roll();
	
	m_Vel.x = m_Vel.x*(1.0f/RampValue);

	if(m_pWorld && m_pWorld->m_Tuning.m_PlayerCollision && m_Roll == 0)
	{
		// check player collision
		float Distance = distance(m_Pos, NewPos);
		int End = Distance+1;
		vec2 LastPos = m_Pos;
		for(int i = 0; i < End; i++)
		{
			float a = i/Distance;
			vec2 Pos = mix(m_Pos, NewPos, a);
			for(int p = 0; p < MAX_CLIENTS; p++)
			{
				CCharacterCore *pCharCore = m_pWorld->m_apCharacters[p];
				if(!pCharCore || pCharCore == this || pCharCore->m_Roll != 0 || pCharCore->Status(STATUS_SPAWNING))
					continue;
				float D = distance(Pos, pCharCore->m_Pos);
				if(D < 28.0f && D > 0.0f)
				{
					if(a > 0.0f)
						m_Pos = LastPos;
					else if(distance(NewPos, pCharCore->m_Pos) > D)
						m_Pos = NewPos;
					return;
				}
			}
			LastPos = Pos;
		}
	}
	
	// monster collision
	/*
	if(m_pWorld)
	{
		float Distance = distance(m_Pos, NewPos);
		int End = Distance+1;
		vec2 LastPos = m_Pos;
		for(int i = 0; i < End; i++)
		{
			float a = i/Distance;
			vec2 Pos = mix(m_Pos, NewPos, a);
			for(int p = 0; p < MAX_MONSTERS; p++)
			{
				vec2 MonsterPos = m_pWorld->m_aMonsterPos[p];
				
				if(MonsterPos.x == 0)
					continue;
				
				float D = distance(Pos, MonsterPos);
				if(D < 28.0f && D > 0.0f)
				{
					if(a > 0.0f)
						m_Pos = LastPos;
					else if(distance(NewPos, MonsterPos) > D)
						m_Pos = NewPos;

					return;
					
					// for testing purposes
					//m_Status |= 1 << STATUS_ELECTRIC;
				}
			}
			LastPos = Pos;
		}
	}
	*/
	

	m_Pos = NewPos;
}

void CCharacterCore::Write(CNetObj_CharacterCore *pObjCore)
{
	pObjCore->m_X = round_to_int(m_Pos.x);
	pObjCore->m_Y = round_to_int(m_Pos.y);

	pObjCore->m_VelX = round_to_int(m_Vel.x*256.0f);
	pObjCore->m_VelY = round_to_int(m_Vel.y*256.0f);
	pObjCore->m_HookState = m_HookState;
	pObjCore->m_HookTick = m_HookTick;
	pObjCore->m_HookX = round_to_int(m_HookPos.x);
	pObjCore->m_HookY = round_to_int(m_HookPos.y);
	pObjCore->m_HookDx = round_to_int(m_HookDir.x*256.0f);
	pObjCore->m_HookDy = round_to_int(m_HookDir.y*256.0f);
	pObjCore->m_HookedPlayer = m_HookedPlayer;
	pObjCore->m_DamageTick = m_DamageTick;
	pObjCore->m_Jumped = m_Jumped;
	pObjCore->m_Direction = m_Direction;
	pObjCore->m_Sliding = m_Sliding;
	pObjCore->m_Grounded = IsGrounded();
	pObjCore->m_Angle = m_Angle;
	pObjCore->m_Anim = m_Anim;
	pObjCore->m_Jetpack = m_Jetpack;
	pObjCore->m_HandJetpack = m_HandJetpack;
	pObjCore->m_JetpackPower = m_JetpackPower;
	pObjCore->m_Wallrun = m_Wallrun;
	pObjCore->m_Roll = m_Roll;
	pObjCore->m_Status = m_Status;
	pObjCore->m_LockDirection = m_LockDirection;
	pObjCore->m_Slope = SlopeState();
}

void CCharacterCore::Read(const CNetObj_CharacterCore *pObjCore)
{
	m_Pos.x = pObjCore->m_X;
	m_Pos.y = pObjCore->m_Y;
	m_Vel.x = pObjCore->m_VelX/256.0f;
	m_Vel.y = pObjCore->m_VelY/256.0f;
	m_HookState = pObjCore->m_HookState;
	m_HookTick = pObjCore->m_HookTick;
	m_HookPos.x = pObjCore->m_HookX;
	m_HookPos.y = pObjCore->m_HookY;
	m_HookDir.x = pObjCore->m_HookDx/256.0f;
	m_HookDir.y = pObjCore->m_HookDy/256.0f;
	m_HookedPlayer = pObjCore->m_HookedPlayer;
	m_DamageTick = pObjCore->m_DamageTick;
	m_Jumped = pObjCore->m_Jumped;
	m_Direction = pObjCore->m_Direction;
	m_Sliding = pObjCore->m_Sliding;
	m_Angle = pObjCore->m_Angle;
	m_Anim = pObjCore->m_Anim;
	m_Jetpack = pObjCore->m_Jetpack;
	m_HandJetpack = pObjCore->m_HandJetpack;
	m_JetpackPower = pObjCore->m_JetpackPower;
	m_Wallrun = pObjCore->m_Wallrun;
	m_Roll = pObjCore->m_Roll;
	m_Status = pObjCore->m_Status;
	m_LockDirection = pObjCore->m_LockDirection;
}

void CCharacterCore::Quantize()
{
	CNetObj_CharacterCore Core;
	Write(&Core);
	Read(&Core);
}
