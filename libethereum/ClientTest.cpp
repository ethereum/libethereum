/*
	This file is part of cpp-ethereum.

	cpp-ethereum is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	cpp-ethereum is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with cpp-ethereum.  If not, see <http://www.gnu.org/licenses/>.
*/
/** @file ClientTest.cpp
 * @author Dimitry Khokhlov <dimitry@ethdev.com>
 * @date 2016
 */

#include <libethereum/EthereumHost.h>
#include <libethereum/ClientTest.h>
using namespace std;
using namespace dev;
using namespace dev::eth;
using namespace p2p;

ClientTest& dev::eth::asClientTest(Interface& _c)
{
	return dynamic_cast<ClientTest&>(_c);
}

ClientTest* dev::eth::asClientTest(Interface* _c)
{
	return &dynamic_cast<ClientTest&>(*_c);
}

DEV_SIMPLE_EXCEPTION(ChainParamsNotEthash);

ClientTest::ClientTest(
	ChainParams const& _params,
	int _networkID,
	p2p::Host* _host,
	std::shared_ptr<GasPricer> _gpForAdoption,
	std::string const& _dbPath,
	WithExisting _forceAction,
	TransactionQueue::Limits const& _limits
):
	Client(_params, _networkID, _host, _gpForAdoption, _dbPath, _forceAction, _limits)
{
}

bool ClientTest::addBlock(string const& _rlp)
{
	if (auto h = m_host.lock())
		h->noteNewBlocks();

	bytes rlpBytes = fromHex(_rlp.substr(0, 2) == "0x" ? _rlp.substr(2) : _rlp, WhenError::Throw);
	RLP blockRLP (rlpBytes);
	return (m_bq.import(blockRLP.data(), true) == ImportResult::Success);
}

void ClientTest::rewindToBlock(int _number)
{
	m_bc.rewind(_number);
}

void ClientTest::mineBlocks(int _count, int _timestamp)
{
	m_blockCount = _count;
	m_blockMineTimestamp = _timestamp;
	startSealing();
}

bool ClientTest::submitSealed(bytes const& _header)
{
	BlockHeader previousHeader(bc().block());
	BlockHeader header(_header, HeaderData);

	header.setTimestamp(previousHeader.timestamp() + m_blockMineTimestamp);
	RLPStream s;
	header.streamRLP(s);

	BlockHeader& m_workingHeader = const_cast<BlockHeader&>(m_working.info());
	m_workingHeader = header; //for m_working.sealBlock

	bytes newBlock;
	{
		UpgradableGuard l(x_working);
		{
			UpgradeGuard l2(l);
			if (!m_working.sealBlock(s.out()))
				return false;
		}
		DEV_WRITE_GUARDED(x_postSeal)
			m_postSeal = m_working;
		newBlock = m_working.blockData();
	}

	// OPTIMISE: very inefficient to not utilise the existing OverlayDB in m_postSeal that contains all trie changes.
	return m_bq.import(&newBlock, true) == ImportResult::Success;
}

void ClientTest::doWork(bool _doWait)
{
	bool t = true;
	if (m_syncBlockQueue.compare_exchange_strong(t, false))
		syncBlockQueue();

	if (m_needStateReset)
	{
		resetState();
		m_needStateReset = false;
	}

	t = true;
	bool isSealed;
	DEV_READ_GUARDED(x_working)
		isSealed = m_working.isSealed();
	if (!isSealed && !isSyncing() && !m_remoteWorking && m_syncTransactionQueue.compare_exchange_strong(t, false))
		syncTransactionQueue();

	tick();

	(void)_doWait;
	/*if (!m_syncBlockQueue && !m_syncTransactionQueue && _doWait)
	{
		std::unique_lock<std::mutex> l(x_signalled);
		m_signalled.wait_for(l, chrono::seconds(1));
	}*/
}

void ClientTest::onNewBlocks(h256s const& _blocks, h256Hash& io_changed)
{
	// remove transactions from m_tq nicely rather than relying on out of date nonce later on.
	for (auto const& h: _blocks)
	{
		clog(ClientTrace) << "Live block:" << h;
		cnote << "Live block:" << bc().headerData(h);
	}

	if (auto h = m_host.lock())
		h->noteNewBlocks();

	for (auto const& h: _blocks)
		appendFromBlock(h, BlockPolarity::Live, io_changed);

	if(--m_blockCount <= 0)
	{
		//stop mining
		m_wouldSeal = false;
		rejigSealing();
	}
}
