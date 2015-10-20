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
/** @file Client.cpp
 * @author Gav Wood <i@gavwood.com>
 * @date 2014
 */

#include "FluidityClient.h"

#include <boost/filesystem.hpp>
#include <libdevcore/Common.h>
#include <libdevcore/RLP.h>
#include <libdevcore/FileSystem.h>
#include <libethcore/Exceptions.h>
#include <libethcore/BlockInfo.h>
#include <libethcore/Params.h>
#include <liblll/Compiler.h>
using namespace std;
using namespace dev;
using namespace dev::eth;

unique_ptr<Fluidity::BlockHeader> CanonBlockChain<Fluidity>::s_genesis;
boost::shared_mutex CanonBlockChain<Fluidity>::x_genesis;
u256 CanonBlockChain<Fluidity>::s_genesisGasLimit;
bytes CanonBlockChain<Fluidity>::s_genesisExtraData;

CanonBlockChain<Fluidity>::CanonBlockChain(std::string const& _path, WithExisting _we, ProgressCallback const& _pc):
	FullBlockChain<Fluidity>(createGenesisBlock(), createGenesisState(), _path)
{
	BlockChain::openDatabase(_path, _we, _pc);
}

void CanonBlockChain<Fluidity>::reopen(WithExisting _we, ProgressCallback const& _pc)
{
	close();
	open(createGenesisBlock(), createGenesisState(), m_dbPath);
	openDatabase(m_dbPath, _we, _pc);
}

bytes CanonBlockChain<Fluidity>::createGenesisBlock()
{
	RLPStream block(3);

	h256 stateRoot;
	{
		MemoryDB db;
		SecureTrieDB<Address, MemoryDB> state(&db);
		state.init();
		auto gs = createGenesisState();
		//stateRoot = hash256(toBytesMap(gs));
		dev::eth::commit(gs, state);
		stateRoot = state.root();
	}

	block.appendList(Fluidity::BlockHeader::Fields)
			<< h256() << EmptyListSHA3 << h160() << stateRoot << EmptyTrie << EmptyTrie << LogBloom() << 1 << 0 << (s_genesisGasLimit ? s_genesisGasLimit : (u256(1) << 32)) << 0 << (unsigned)0 << std::string();
	bytes sealFields = typename Fluidity::BlockHeader().sealFieldsRLP();
	block.appendRaw(sealFields, Fluidity::BlockHeader::SealFields);

	block.appendRaw(RLPEmptyList);
	block.appendRaw(RLPEmptyList);
	return block.out();
}

AccountMap const& CanonBlockChain<Fluidity>::createGenesisState()
{
	static AccountMap s_ret;
	if (s_ret.empty())
	{
		s_ret[Address(0)] = Account(0, Account::ContractConception);
		s_ret[Address(0)].setCode(compileLLL("{(mstore 32 32) (return 32 32)}", false));
		s_ret[Address(0)].setStorage(0, 69);
	}
	return s_ret;
}

void CanonBlockChain<Fluidity>::forceGenesisExtraData(bytes const& _genesisExtraData)
{
	WriteGuard l(x_genesis);
	s_genesisExtraData = _genesisExtraData;
	s_genesis.reset();
}

void CanonBlockChain<Fluidity>::forceGenesisGasLimit(u256 const& _genesisGasLimit)
{
	WriteGuard l(x_genesis);
	s_genesisGasLimit = _genesisGasLimit;
	s_genesis.reset();
}

Fluidity::BlockHeader const& CanonBlockChain<Fluidity>::genesis()
{
	UpgradableGuard l(x_genesis);
	if (!s_genesis)
	{
		auto gb = createGenesisBlock();
		UpgradeGuard ul(l);
		s_genesis.reset(new Fluidity::BlockHeader);
		s_genesis->populate(&gb, CheckEverything);
	}
	return *s_genesis;
}
