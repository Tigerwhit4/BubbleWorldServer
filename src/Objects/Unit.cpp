/**
 * Copyright (C) 2016 Martin Ubl <http://kennny.cz>
 *
 * This file is part of BubbleWorld MMORPG engine
 *
 * BubbleWorld is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * BubbleWorld is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with BubbleWorld. If not, see <http://www.gnu.org/licenses/>.
 **/

#include "General.h"
#include "Unit.h"
#include "Opcodes.h"
#include "SmartPacket.h"
#include "Log.h"

Unit::Unit(ObjectType type) : WorldObject(type)
{
    m_moveMask = 0;
    m_moveHeartbeatTimer = 0;
    m_lastMovementUpdate = 0;
}

Unit::~Unit()
{
    //
}

void Unit::Create(uint64_t guid)
{
    WorldObject::Create(guid);
}

void Unit::Update()
{
    WorldObject::Update();

    // update movement if the unit is moving
    if (IsMoving())
    {
        const uint32_t msNow = getMSTime();

        // update position using movement vector
        uint32_t msDiff = getMSTimeDiff(m_lastMovementUpdate, msNow);
        if (msDiff >= 1)
        {
            SetPosition(GetPositionX() + (float)msDiff * m_moveVector.x, GetPositionY() + (float)msDiff * m_moveVector.y);
            m_lastMovementUpdate = msNow;
        }

        // send movement heartbeat to sorroundings if needed
        if (getMSTimeDiff(m_moveHeartbeatTimer, msNow) >= MOVEMENT_HEARTBEAT_SEND_DELAY)
        {
            SmartPacket pkt(SP_MOVE_HEARTBEAT);
            pkt.WriteUInt64(GetGUID());
            pkt.WriteUInt8(m_moveMask);
            pkt.WriteFloat(GetPositionX());
            pkt.WriteFloat(GetPositionY());
            SendPacketToSorroundings(pkt);

            m_moveHeartbeatTimer = msNow;
        }
    }
}

void Unit::BuildCreatePacketBlock(SmartPacket &pkt)
{
    WorldObject::BuildCreatePacketBlock(pkt);

    pkt.WriteFloat(m_position.x);
    pkt.WriteFloat(m_position.y);
    pkt.WriteUInt8(m_moveMask);
}

void Unit::CreateUpdateFields()
{
    // Unit is also not a standalone class

    SetFloatValue(UNIT_FIELD_MOVEMENT_SPEED, 4.0f);

    WorldObject::CreateUpdateFields();
}

uint16_t Unit::GetLevel()
{
    return GetUInt32Value(UNIT_FIELD_LEVEL);
}

void Unit::SetLevel(uint16_t lvl, bool onLoad)
{
    SetUInt32Value(UNIT_FIELD_LEVEL, lvl);

    if (onLoad)
        return;

    // TODO: things related to levelup / leveldown ?
}

void Unit::StartMoving(MoveDirectionElement dir)
{
    if ((m_moveMask & dir) != 0)
        return;

    SmartPacket pkt(SP_MOVE_START_DIRECTION);
    pkt.WriteUInt64(GetGUID());
    pkt.WriteUInt8(dir);
    SendPacketToSorroundings(pkt);

    // if started movement, set timers for movement update
    if (m_moveMask == 0)
    {
        m_lastMovementUpdate = getMSTime();
        m_moveHeartbeatTimer = getMSTime();
    }

    m_moveMask |= dir;
    UpdateMovementVector();
}

void Unit::StopMoving(MoveDirectionElement dir)
{
    if ((m_moveMask & dir) == 0)
        return;

    SmartPacket pkt(SP_MOVE_STOP_DIRECTION);
    pkt.WriteUInt64(GetGUID());
    pkt.WriteUInt8(dir);
    pkt.WriteFloat(GetPositionX());
    pkt.WriteFloat(GetPositionY());
    SendPacketToSorroundings(pkt);

    m_moveMask &= ~dir;
    UpdateMovementVector();
}

bool Unit::IsMoving()
{
    return m_moveMask != 0;
}

void Unit::UpdateMovementVector()
{
    // every fifth state (0000, 0101, 1010, 1111) cancels the movement
    if (m_moveMask % 5 == 0)
    {
        m_moveVector.x = 0;
        m_moveVector.y = 0;
    }
    else
    {
        // update vecotr using polar coordinates
        m_moveVector.SetFromPolar(movementAngles[m_moveMask], GetFloatValue(UNIT_FIELD_MOVEMENT_SPEED));
        m_moveVector.x *= MOVEMENT_UPDATE_UNIT_FRACTION;
        m_moveVector.y *= MOVEMENT_UPDATE_UNIT_FRACTION;
    }
}

void Unit::Talk(TalkType type, const char* str)
{
    SmartPacket pkt(SP_CHAT_MESSAGE);
    pkt.WriteUInt8(type);
    pkt.WriteUInt64(GetGUID());
    pkt.WriteString(str);
    SendPacketToSorroundings(pkt);
}