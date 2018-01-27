#pragma once

#include "messages.h"
#include "receiver.h"
#include "template_dispatcher.h"

namespace messaging
{

unsigned bank_machine_balance;

class bank_machine
{
	messaging::receiver incoming;
	
public:
	bank_machine(){
		bank_machine_balance = 199;
	}

	void done()
	{
		get_sender().send(messaging::close_queue());
	}

	void run()
	{
		try
		{
			for (;;)
			{
				incoming.wait().handle<verify_pin>([&](verify_pin const& msg)
					{
						if (msg.pin == "1937")
						{
							msg.atm_queue.send(pin_verified());
						}
						else
						{
							msg.atm_queue.send(pin_incorrect());
						}
					}
				).handle<withdraw>([&](withdraw const& msg)
					{
						if (bank_machine_balance >= msg.amount)
						{
							msg.atm_queue.send(::withdraw_ok());
							bank_machine_balance -= msg.amount;
						}
						else
						{
							msg.atm_queue.send(withdraw_denied());
						}
					}
				).handle<get_balance>([&](get_balance const& msg)
					{
						msg.atm_queue.send(::balance(bank_machine_balance));
					}
				).handle<withdrawal_processed>([&](withdrawal_processed const& msg)
					{
					}
				).handle<cancel_withdrawal>([&](cancel_withdrawal const& msg)
					{
					}
				);
			}
		}
		catch (messaging::close_queue const&)
		{
		}
	}

	messaging::sender get_sender()
	{
		return incoming;
	}
};

}