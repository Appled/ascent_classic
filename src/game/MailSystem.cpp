/*
 * Ascent MMORPG Server
 * Copyright (C) 2005-2007 Ascent Team <http://www.ascentemu.com/>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "StdAfx.h"
initialiseSingleton(MailSystem);

void MailSystem::StartMailSystem()
{

}

MailError MailSystem::DeliverMessage(uint64 recipent, MailMessage* message)
{
	// assign a new id
	message->message_id = Generate_Message_Id();

	Player * plr = objmgr.GetPlayer((uint32)recipent);
	if(plr != NULL)
	{
		plr->m_mailBox.AddMessage(message);
		if((uint32)UNIXTIME >= message->delivery_time)
		{
			uint32 v = 0;
			plr->GetSession()->OutPacket(SMSG_RECEIVED_MAIL, 4, &v);
		}
	}

	SaveMessageToSQL(message);
	return MAIL_OK;
}

void Mailbox::AddMessage(MailMessage* Message)
{
	Messages[Message->message_id] = *Message;
}

void Mailbox::DeleteMessage(uint32 MessageId, bool sql)
{
	Messages.erase(MessageId);
	if(sql)
		CharacterDatabase.WaitExecute("DELETE FROM mailbox WHERE message_id = %u", MessageId);
}

WorldPacket * Mailbox::BuildMailboxListingPacket()
{
	WorldPacket * data = new WorldPacket(SMSG_MAIL_LIST_RESULT, 500);
	MessageMap::iterator itr;
	uint32 count = 0;
	uint32 t = (uint32)UNIXTIME;
	*data << uint8(0);	 // size placeholder

	for(itr = Messages.begin(); itr != Messages.end(); ++itr)
	{
		if(itr->second.expire_time && t > itr->second.expire_time)
			continue;	   // expired mail -> skip it

		if(UNIXTIME < itr->second.delivery_time)
			continue;		// undelivered
		
		if(itr->second.AddMessageDataToPacket(*data))
			++count;
		
		if(count == 50)
			break;
	}

	const_cast<uint8*>(data->contents())[0] = count;

	// do cleanup on request mail
	CleanupExpiredMessages();
	return data;
}

void Mailbox::CleanupExpiredMessages()
{
	MessageMap::iterator itr, it2;
	uint32 curtime = (uint32)UNIXTIME;

	for(itr = Messages.begin(); itr != Messages.end();)
	{
		it2 = itr++;
		if(it2->second.expire_time && it2->second.expire_time < curtime)
		{
			Messages.erase(it2);
		}
	}
}

bool MailMessage::AddMessageDataToPacket(WorldPacket& data)
{
	// add stuff
	if(deleted_flag)
		return false;

	data << uint16(0x0032);
	data << message_id;
	data << uint8(message_type);
	if(message_type)
		data << uint32(sender_guid);
	else
		data << sender_guid;

	data << cod;			// cod
	data << message_id;		// itempageid
	data << uint32(0);
	data << stationary;
	data << money;		// money
	data << uint32(0x10);
	data << float(float(expire_time - (uint32)UNIXTIME) / 86400.0f);
	data << uint32(0);
	data << subject;
	//data << uint8(0);		// item count

	uint32 itementry = 0, itemcount = 0;
	uint32 charges = 0, durability = 0, maxdurability = 0;

	if(attached_item_guid)
	{
		QueryResult * result = CharacterDatabase.Query("SELECT `entry`, `count`, `charges`, `durability` FROM playeritems WHERE guid='%u'", GUID_LOPART(attached_item_guid));
		if(result)
		{
			itementry = result->Fetch()[0].GetUInt32();
			itemcount = result->Fetch()[1].GetUInt32();
			charges = result->Fetch()[2].GetUInt32();
			durability = result->Fetch()[3].GetUInt32();
			ItemPrototype * it = ItemPrototypeStorage.LookupEntry(itementry);
			maxdurability = it ? it->MaxDurability : durability;

			delete result;
		}
	}

	if(external_attached_item_guid)
	{
		QueryResult * result = CharacterDatabase.Query("SELECT `entry`, `count`, `charges`, `durability` FROM playeritems_external WHERE guid='%u'", GUID_LOPART(external_attached_item_guid));
		if(result)
		{
			itementry = result->Fetch()[0].GetUInt32();
			itemcount = result->Fetch()[1].GetUInt32();
			charges = result->Fetch()[2].GetUInt32();
			durability = result->Fetch()[3].GetUInt32();
			ItemPrototype * it = ItemPrototypeStorage.LookupEntry(itementry);
			maxdurability = it ? it->MaxDurability : durability;

			delete result;
		}
	}

	if(itementry)
	{
		/*
		00
		C5 85 0D 00
		A6 01 00 00
		00 00 00 00 
		10 62 3B 47
		00 00 00 00
		00 00 00 00
		10 62 3B 47
		00 00 00 00
		00 00 00 00
		10 62 3B 47
		00 00 00 00
		00 00 00 00
		10 62 3B 47
		00 00 00 00
		00 00 00 00
		10 62 3B 47
		00 00 00 00
		00 00 00 00
		10 62 3B 47
		00 00 00 00
		00 00 00 00
		88 AF CA 62
		04
		FF FF FF FF
		00 00 00 00
		00 00 00 00
		*/
		data << uint8(1);		// one item
		data << uint8(0);
		data << uint32(0);			// something (0x000D85C5)
		data << itementry;
		data << uint32(0);
		data << uint32(0);
		data << uint32(0);
		data << uint32(0);
		data << uint32(0);
		data << uint32(0);
		data << uint32(0);
		data << uint32(0);
		data << uint32(0);
		data << uint32(0);
		data << uint32(0);
		data << uint32(0);
		data << uint32(0);
		data << uint32(0);
		data << uint32(0);
		data << uint32(0);
		data << uint32(0);
		data << uint32(0);
		data << uint32(0);
		data << uint32(0);
		data << uint8(itemcount ? itemcount : 1);
		data << uint32(charges);
		data << uint32(maxdurability);
		data << uint32(durability);
	}
	else
		data << uint8(0);

	return true;
/*
	data << uint16(0);
	data << message_id;
	data << uint8(message_type);
	if(message_type)
		data << uint32(sender_guid);
	else
		data << sender_guid;

	data << subject;
	data << message_id;	  // itempageid
	data << message_id;

	data << stationary;

	uint32 itementry = 0, itemcount = 0;
	uint32 charges = 0, durability = 0, maxdurability = 0;

	if(attached_item_guid)
	{
		QueryResult * result = CharacterDatabase.Query("SELECT `entry`, `count`, `charges`, `durability` FROM playeritems WHERE guid='%u'", GUID_LOPART(attached_item_guid));
		if(result)
		{
			itementry = result->Fetch()[0].GetUInt32();
			itemcount = result->Fetch()[1].GetUInt32();
			charges = result->Fetch()[2].GetUInt32();
			durability = result->Fetch()[3].GetUInt32();
			ItemPrototype * it = ItemPrototypeStorage.LookupEntry(itementry);
			maxdurability = it ? it->MaxDurability : durability;

			delete result;
		}
	}

	if(external_attached_item_guid)
	{
		QueryResult * result = CharacterDatabase.Query("SELECT `entry`, `count`, `charges`, `durability` FROM playeritems_external WHERE guid='%u'", GUID_LOPART(external_attached_item_guid));
		if(result)
		{
			itementry = result->Fetch()[0].GetUInt32();
			itemcount = result->Fetch()[1].GetUInt32();
			charges = result->Fetch()[2].GetUInt32();
			durability = result->Fetch()[3].GetUInt32();
			ItemPrototype * it = ItemPrototypeStorage.LookupEntry(itementry);
			maxdurability = it ? it->MaxDurability : durability;

			delete result;
		}
	}

	data << itementry;
	data << uint32(0);  // unk
	data << uint32(0);  // unk
	data << uint32(0);  // unk
	for(uint32 i = 0; i < 17; ++i)
		data << uint32(0);

	data << (itemcount ? uint8(itemcount) : uint8(1));
	data << charges;
	data << maxdurability;
	data << durability;
	data << money;
	data << cod;
	data << uint32(read_flag);

	if(expire_time)
		data << float(float(expire_time - (uint32)UNIXTIME) / 86400.0f);
	else
		data << float(0);

	data << uint32(0);

	return true;*/
}

void MailSystem::SaveMessageToSQL(MailMessage * message)
{
	stringstream ss;
	ss << "INSERT INTO mailbox VALUES("
		<< message->message_id << ","
		<< message->message_type << ","
		<< message->player_guid << ","
		<< message->sender_guid << ",\""
		<< CharacterDatabase.EscapeString(message->subject) << "\",\""
		<< CharacterDatabase.EscapeString(message->body) << "\","
		<< message->money << ",'"
		<< message->attached_item_guid << "','"
		<< message->external_attached_item_guid << "',"
		<< message->cod << ","
		<< message->stationary << ","
		<< message->expire_time << ","
		<< message->delivery_time << ","
		<< message->copy_made << ","
		<< message->read_flag << ","
		<< message->deleted_flag << ")";
	CharacterDatabase.WaitExecute(ss.str().c_str());
}

void WorldSession::HandleSendMail(WorldPacket & recv_data )
{
	MailMessage msg;
	uint64 gameobject;
	uint32 unk2;
	uint8 itemcount;
	string recepient;
	//uint32 err = MAIL_OK;

	recv_data >> gameobject >> recepient;
	recv_data >> msg.subject >> msg.body >> msg.stationary;
	recv_data >> unk2 >> itemcount;

	if(itemcount>1)
	{
		SystemMessage("Sorry, Ascent does not support sending multiple items at this time. (don't want to lose your item do you) Remove some items, and try again.");
		SendMailError(MAIL_ERR_INTERNAL_ERROR);
		return;
	}
	
	if(itemcount)
	{
		recv_data >> itemcount;		// bs field
		recv_data >> msg.attached_item_guid;
	}
	else
		msg.attached_item_guid=0;

	recv_data >> msg.money;
	recv_data >> msg.cod;
	// left over: (TODO- FIX ME BURLEX!)
	// uint32
	// uint32
	// uint8
	
	// Search for the recipient
	PlayerInfo * player = ObjectMgr::getSingleton().GetPlayerInfoByName(recepient);
	if(player == 0)
	{
		SendMailError(MAIL_ERR_RECIPIENT_NOT_FOUND);
		return;
	}

	bool interfaction = false;
	if(sMailSystem.MailOption(MAIL_FLAG_CAN_SEND_TO_OPPOSITE_FACTION) ||
		(HasGMPermissions() && sMailSystem.MailOption(MAIL_FLAG_CAN_SEND_TO_OPPOSITE_FACTION_GM)))
	{
		interfaction = true;
	}

	// Check we're sending to the same faction (disable this for testing)
	if(player->team != _player->GetTeam() && !interfaction)
	{
		SendMailError(MAIL_ERR_NOT_YOUR_ALLIANCE);
		return;
	}

	// Check if we're sending mail to ourselves
	if(player->name == _player->GetName() && !GetPermissionCount())
	{
		SendMailError(MAIL_ERR_CANNOT_SEND_TO_SELF);
		return;
	}

	// Instant delivery time by default.
	msg.delivery_time = (uint32)UNIXTIME;

	// Set up the cost
	int32 cost = 0;
	if(!sMailSystem.MailOption(MAIL_FLAG_DISABLE_POSTAGE_COSTS) &&
		!(GetPermissionCount() && sMailSystem.MailOption(MAIL_FLAG_NO_COST_FOR_GM)))
	{
		cost = 30;
	}

	// Check for attached money
	if(msg.money > 0)
		cost += msg.money;

	// check that we have enough in our backpack
	if((int32)_player->GetUInt32Value(PLAYER_FIELD_COINAGE) < cost)
	{
		SendMailError(MAIL_ERR_NOT_ENOUGH_MONEY);
		return;
	}

	// Check for the item, and required item.
	if(msg.attached_item_guid != 0)
	{
		Item * attached_item = _player->GetItemInterface()->SafeRemoveAndRetreiveItemByGuid(msg.attached_item_guid, false);
		if(attached_item == 0 || attached_item->IsSoulbound())
		{
			// could not find item
			SendMailError(MAIL_ERR_INTERNAL_ERROR);
			return;
		}

		// Save the item with no owner (to objectmgr)
		attached_item->RemoveFromWorld();
		attached_item->SetOwner(NULL);
		attached_item->SaveToDB(INVENTORY_SLOT_NOT_SET, 0,true);

		// Cut out the high part of the attached item.
		msg.attached_item_guid = attached_item->GetGUID();

		if(GetPermissionCount() > 0)
		{
			/* log the message */
			sGMLog.writefromsession(this, "sent mail with item entry %u to %s, with gold %u.", attached_item->GetEntry(), player->name, msg.money);
		}

		// Delete from memory now, its not needed anymore.
		delete attached_item;

		// no need to update guid, it shouldn't change..
	}
	msg.external_attached_item_guid=0;

	if(msg.money != 0 || msg.cod != 0 || msg.attached_item_guid != 0 && player->acct != _player->GetSession()->GetAccountId())
	{
		if(!sMailSystem.MailOption(MAIL_FLAG_DISABLE_HOUR_DELAY_FOR_ITEMS))
			msg.delivery_time += 3600;  // 1hr
	}

	// take the money
	_player->ModUInt32Value(PLAYER_FIELD_COINAGE, -cost);

	// Fill in the rest of the info
	msg.player_guid = player->guid;
	msg.sender_guid = _player->GetGUID();
	
	// 30 day expiry time for unread mail mail
	if(!sMailSystem.MailOption(MAIL_FLAG_NO_EXPIRY))
		msg.expire_time = (uint32)UNIXTIME + (TIME_DAY * 30);
	else
		msg.expire_time = 0;

	msg.copy_made = false;
	msg.read_flag = false;
	msg.deleted_flag = false;
	msg.message_type = 0;

	// Great, all our info is filled in. Now we can add it to the other players mailbox.
	sMailSystem.DeliverMessage(player->guid, &msg);

	// Success packet :)
	SendMailError(MAIL_OK);
}

void WorldSession::HandleMarkAsRead(WorldPacket & recv_data )
{
	uint64 mailbox;
	uint32 message_id;
	recv_data >> mailbox >> message_id;

	MailMessage * message = _player->m_mailBox.GetMessage(message_id);
	if(message == 0) return;

	// mark the message as read
	message->read_flag = 1;

	// mail now has a 3 day expiry time
	if(!sMailSystem.MailOption(MAIL_FLAG_NO_EXPIRY))
		message->expire_time = (uint32)UNIXTIME + (TIME_DAY * 3);

	// update it in sql
	CharacterDatabase.WaitExecute("UPDATE mailbox SET read_flag = 1, expiry_time = %u WHERE message_id = %u", message->message_id, message->expire_time);
}

void WorldSession::HandleMailDelete(WorldPacket & recv_data )
{
	uint64 mailbox;
	uint32 message_id;
	recv_data >> mailbox >> message_id;

	WorldPacket data(SMSG_SEND_MAIL_RESULT, 12);
	data << message_id << uint32(MAIL_RES_DELETED);

	MailMessage * message = _player->m_mailBox.GetMessage(message_id);
	if(message == 0)
	{
		data << uint32(MAIL_ERR_INTERNAL_ERROR);
		SendPacket(&data);

		return;
	}

	if(message->copy_made)
	{
		// we have the message as a copy on the item. we can't delete it or this item
		// will no longer function.

		// deleted_flag prevents it from being shown in the mail list.
		message->deleted_flag = 1;

		// update in sql
		CharacterDatabase.WaitExecute("UPDATE mailbox SET deleted_flag = 1 WHERE message_id = %u", message_id);
	}
	else
	{
		// delete the message, there are no other references to it.
		_player->m_mailBox.DeleteMessage(message_id, true);
	}

	data << uint32(MAIL_OK);
	SendPacket(&data);
}

void WorldSession::HandleTakeItem(WorldPacket & recv_data )
{
	uint64 mailbox;
	uint32 message_id;
	recv_data >> mailbox >> message_id;

	WorldPacket data(SMSG_SEND_MAIL_RESULT, 12);
	data << message_id << uint32(MAIL_RES_ITEM_TAKEN);
	
	MailMessage * message = _player->m_mailBox.GetMessage(message_id);
	if(message == 0 || (!message->attached_item_guid && !message->external_attached_item_guid))
	{
		data << uint32(MAIL_ERR_INTERNAL_ERROR);
		SendPacket(&data);

		return;
	}

	// check for cod credit
	if(message->cod > 0)
	{
		if(_player->GetUInt32Value(PLAYER_FIELD_COINAGE) < message->cod)
		{
			data << uint32(MAIL_ERR_NOT_ENOUGH_MONEY);
			SendPacket(&data);
			return;
		}
	}

	// grab the item
	Item * item = NULL;

	if(message->attached_item_guid)
		item = objmgr.LoadItem(message->attached_item_guid);
	else if(message->external_attached_item_guid)
	{
		item = objmgr.LoadExternalItem(message->external_attached_item_guid);
		if(item)
		{
			// reassign its guid
			if(item->GetProto()->ContainerSlots)
				item->SetNewGuid(objmgr.GenerateLowGuid(HIGHGUID_CONTAINER));
			else
				item->SetNewGuid(objmgr.GenerateLowGuid(HIGHGUID_ITEM));
		}
	}

	if(item == 0)
	{
		// doesn't exist
		data << uint32(MAIL_ERR_INTERNAL_ERROR);
		SendPacket(&data);
		
		return;
	}

	// find a free bag slot
	SlotResult result = _player->GetItemInterface()->FindFreeInventorySlot(item->GetProto());
	if(result.Result == 0)
	{
		// no free slots left!
		data << uint32(MAIL_ERR_BAG_FULL);
		SendPacket(&data);

		delete item;
		return;
	}

	// all is good
	// delete the item (so when its resaved it'll have an association)
	item->DeleteFromDB();

	// add the item to their backpack
	_player->GetItemInterface()->AddItemToFreeSlot(item);

	// message no longer has an item association
	if(message->external_attached_item_guid)
	{
		CharacterDatabase.Execute("DELETE FROM playeritems_external WHERE guid = "I64FMTD, message->external_attached_item_guid);
		message->external_attached_item_guid = 0;
	}
	else
		message->attached_item_guid = 0;
	
	// update in sql!
	CharacterDatabase.WaitExecute("UPDATE mailbox SET attached_item_guid = 0, external_attached_item_guid = 0, cod = 0 WHERE message_id = %u", message->message_id);

	// send complete packet
	data << uint32(MAIL_OK);
	SendPacket(&data);
	
	if( message->cod > 0 )
	{
		_player->ModUInt32Value(PLAYER_FIELD_COINAGE, -int32(message->cod));
		string subject = "COD Payment: ";
		subject += message->subject;
		sMailSystem.SendAutomatedMessage(NORMAL, message->player_guid, message->sender_guid, subject, "", message->cod, 0, 0, 1);
	}

	// prolly need to send an item push here
}

void WorldSession::HandleTakeMoney(WorldPacket & recv_data )
{
	uint64 mailbox;
	uint32 message_id;
	recv_data >> mailbox >> message_id;

	WorldPacket data(SMSG_SEND_MAIL_RESULT, 12);
	data << message_id << uint32(MAIL_RES_MONEY_TAKEN);

	MailMessage * message = _player->m_mailBox.GetMessage(message_id);
	if(message == 0 || !message->money)
	{
		data << uint32(MAIL_ERR_INTERNAL_ERROR);
		SendPacket(&data);

		return;
	}

	// add the money to the player
	_player->ModUInt32Value(PLAYER_FIELD_COINAGE, message->money);

	// message no longer has any money
	message->money = 0;

	// update in sql!
	CharacterDatabase.WaitExecute("UPDATE mailbox SET money = 0 WHERE message_id = %u", message->message_id);

	// send result
	data << uint32(MAIL_OK);
	SendPacket(&data);
}

void WorldSession::HandleReturnToSender(WorldPacket & recv_data )
{
	uint64 mailbox;
	uint32 message_id;
	recv_data >> mailbox >> message_id;

	WorldPacket data(SMSG_SEND_MAIL_RESULT, 12);
	data << message_id << uint32(MAIL_RES_RETURNED_TO_SENDER);

	MailMessage * msg = _player->m_mailBox.GetMessage(message_id);
	if(msg == 0)
	{
		data << uint32(MAIL_ERR_INTERNAL_ERROR);
		SendPacket(&data);

		return;
	}
	
	// copy into a new struct
	MailMessage message = *msg;

	// remove the old message
	_player->m_mailBox.DeleteMessage(message_id, true);

	// re-assign the owner/sender
	message.player_guid = message.sender_guid;
	message.sender_guid = _player->GetGUID();

	// turn off the read flag
	message.read_flag = false;
	message.deleted_flag = false;
	message.copy_made = false;

	// null out the cod charges. (the sender doesnt want to have to pay for his own item
	// that he got nothing for.. :p)
	message.cod = 0;

	// assign new delivery time
	message.delivery_time = message.attached_item_guid ? (uint32)UNIXTIME + 3600 : (uint32)UNIXTIME;

	// add to the senders mailbox
	sMailSystem.DeliverMessage(message.player_guid, &message);

	// finish the packet
	data << uint32(MAIL_OK);
	SendPacket(&data);
}

void WorldSession::HandleMailCreateTextItem(WorldPacket & recv_data )
{
	uint64 mailbox;
	uint32 message_id;
	recv_data >> mailbox >> message_id;

	WorldPacket data(SMSG_SEND_MAIL_RESULT, 12);
	data << message_id << uint32(MAIL_RES_MADE_PERMANENT);

	ItemPrototype * proto = ItemPrototypeStorage.LookupEntry(8383);
	MailMessage * message = _player->m_mailBox.GetMessage(message_id);
	if(message == 0 || !proto)
	{
		data << uint32(MAIL_ERR_INTERNAL_ERROR);
		SendPacket(&data);

		return;
	}

	SlotResult result = _player->GetItemInterface()->FindFreeInventorySlot(proto);
	if(result.Result == 0)
	{
		data << uint32(MAIL_ERR_INTERNAL_ERROR);
		SendPacket(&data);

		return;
	}

	Item * pItem = objmgr.CreateItem(8383, _player);
	pItem->SetUInt32Value(ITEM_FIELD_ITEM_TEXT_ID, message_id);
	_player->GetItemInterface()->AddItemToFreeSlot(pItem);

	// mail now has an item after it
	message->copy_made = true;

	// update in sql
	CharacterDatabase.WaitExecute("UPDATE mailbox SET copy_made = 1 WHERE message_id = %u", message_id);

	data << uint32(MAIL_OK);
	SendPacket(&data);
}

void WorldSession::HandleItemTextQuery(WorldPacket & recv_data)
{
	uint32 message_id;
	recv_data >> message_id;

	string body = "Internal Error";

	MailMessage * msg = _player->m_mailBox.GetMessage(message_id);
	if(msg)
		body = msg->body;

	WorldPacket data(SMSG_ITEM_TEXT_QUERY_RESPONSE, body.length() + 5);
	data << message_id << body;
	SendPacket(&data);
}

void Mailbox::FillTimePacket(WorldPacket& data)
{
	uint32 c = 0;
	MessageMap::iterator iter = Messages.begin();
	data << uint32(0) << uint32(0);

	for(; iter != Messages.end(); ++iter)
	{
		if(iter->second.read_flag == 0 && UNIXTIME >= iter->second.delivery_time)
		{
			// unread message, w00t.
			++c;
			data << uint64(iter->second.sender_guid);
			data << uint32(0);
			data << uint32(0);// money or smth?
			data << uint32(iter->second.stationary);
			//data << float(UNIXTIME-iter->second.delivery_time);
			data << float(-9.0f);	// maybe the above?
		}
	}

	if(c==0)
	{
#ifdef USING_BIG_ENDIAN
		*(uint32*)(&data.contents()[0])=swap32(0xc7a8c000);
#else
		*(uint32*)(&data.contents()[0])=0xc7a8c000;
#endif
	}
	else
	{
#ifdef USING_BIG_ENDIAN
		*(uint32*)(&data.contents()[4])=swap32(c);
#else
		*(uint32*)(&data.contents()[4])=c;
#endif
	}
}

void WorldSession::HandleMailTime(WorldPacket & recv_data)
{
	WorldPacket data(MSG_QUERY_NEXT_MAIL_TIME, 100);
	_player->m_mailBox.FillTimePacket(data);
	SendPacket(&data);
}

void WorldSession::SendMailError(uint32 error)
{
	WorldPacket data(SMSG_SEND_MAIL_RESULT, 12);
	data << uint32(0);
	data << uint32(MAIL_RES_MAIL_SENT);
	data << error;
	SendPacket(&data);
}

void WorldSession::HandleGetMail(WorldPacket & recv_data )
{
	WorldPacket * data = _player->m_mailBox.BuildMailboxListingPacket();
	SendPacket(data);
	delete data;
}

void MailSystem::RemoveMessageIfDeleted(uint32 message_id, Player * plr)
{
	MailMessage * msg = plr->m_mailBox.GetMessage(message_id);
	if(msg == 0) return;

	if(msg->deleted_flag)   // we've deleted from inbox
		plr->m_mailBox.DeleteMessage(message_id, true);   // wipe the message
}

void MailSystem::SendAutomatedMessage(uint32 type, uint64 sender, uint64 receiver, string subject, string body,
									  uint32 money, uint32 cod, uint64 item_guid, uint32 stationary)
{
	// This is for sending automated messages, for example from an auction house.
	MailMessage msg;
	msg.message_type = type;
	msg.sender_guid = sender;
	msg.player_guid = receiver;
	msg.subject = subject;
	msg.body = body;
	msg.money = money;
	msg.cod = cod;
	msg.attached_item_guid = item_guid;
	msg.external_attached_item_guid = 0;
	msg.stationary = stationary;
	msg.delivery_time = (uint32)UNIXTIME;
	msg.expire_time = 0;
	msg.read_flag = false;
	msg.copy_made = false;
	msg.deleted_flag = false;

	// Send the message.
	DeliverMessage(receiver, &msg);
}

uint32 MailSystem::Generate_Message_Id()
{
	/** I know this is horrible. But when you have external mail sources unfortunately this is the only way to do this.
	 * - Burlex
	 */

	uint32 id = 1;
	QueryResult * result = CharacterDatabase.Query("SELECT MAX(message_id) FROM mailbox");
	if(result)
	{
		id = result->Fetch()[0].GetUInt32()+1;
		delete result;
	}

	return id;
}

void Mailbox::Load(QueryResult * result)
{
	if(!result)
		return;

	Field * fields;
	MailMessage msg;
	uint32 i;

	do 
	{
		fields = result->Fetch();

		// Create message struct
		i = 0;
		msg.message_id = fields[i++].GetUInt32();
		msg.message_type = fields[i++].GetUInt32();
		msg.player_guid = fields[i++].GetUInt32();
		msg.sender_guid = fields[i++].GetUInt32();
		msg.subject = fields[i++].GetString();
		msg.body = fields[i++].GetString();
		msg.money = fields[i++].GetUInt32();
		msg.attached_item_guid = fields[i++].GetUInt64();
		msg.external_attached_item_guid = fields[i++].GetUInt64();
		msg.cod = fields[i++].GetUInt32();
		msg.stationary = fields[i++].GetUInt32();
		msg.expire_time = fields[i++].GetUInt32();
		msg.delivery_time = fields[i++].GetUInt32();
		msg.copy_made = fields[i++].GetBool();
		msg.read_flag = fields[i++].GetBool();
		msg.deleted_flag = fields[i++].GetBool();

		// Add to the mailbox
		AddMessage(&msg);

	} while(result->NextRow());
}
