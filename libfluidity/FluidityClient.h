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
/** @file Client.h
 * @author Gav Wood <i@gavwood.com>
 * @date 2014
 */

#pragma once

#include <thread>
#include <condition_variable>
#include <mutex>
#include <list>
#include <atomic>
#include <string>
#include <array>
#include <libethereum/Client.h>
#include "Fluidity.h"

namespace dev
{
namespace eth
{

template <>
class CanonBlockChain<Fluidity>: public FullBlockChain<Fluidity>
{
public:
	CanonBlockChain(WithExisting _we = WithExisting::Trust, ProgressCallback const& _pc = ProgressCallback()): CanonBlockChain(std::string(), _we, _pc) {}
	CanonBlockChain(std::string const& _path, WithExisting _we = WithExisting::Trust, ProgressCallback const& _pc = ProgressCallback());
	~CanonBlockChain() {}

	/// Reopen everything.
	virtual void reopen(WithExisting _we = WithExisting::Trust, ProgressCallback const& _pc = ProgressCallback());

	/// @returns the genesis block header.
	static Fluidity::BlockHeader const& genesis();

	/// @returns the genesis block as its RLP-encoded byte array.
	/// @note This is slow as it's constructed anew each call. Consider genesis() instead.
	static bytes createGenesisBlock();

	/// @returns the genesis block's state.
	/// @note This is slow as it's constructed anew each call. Consider genesis() instead.
	static AccountMap const& createGenesisState();

	/// Override the genesis block's gasLimit field.
	static void forceGenesisGasLimit(u256 const& _genesisGasLimit);

	/// Override the genesis block's extraData field.
	static void forceGenesisExtraData(bytes const& _genesisExtraData);

private:
	/// Static genesis info and its lock.
	static boost::shared_mutex x_genesis;
	static std::unique_ptr<Fluidity::BlockHeader> s_genesis;
	static bytes s_genesisExtraData;
	static u256 s_genesisGasLimit;
};

class FluidityClient: public SpecialisedClient<Fluidity>
{
public:
	/// Trivial forwarding constructor.
	explicit FluidityClient(
		p2p::Host* _host,
		std::string const& _dbPath = std::string(),
		WithExisting _forceAction = WithExisting::Trust
	):
		SpecialisedClient<Fluidity>(nullptr, _dbPath, _forceAction)
	{
		init(_host, _dbPath, _forceAction, (int)c_network);
	}
};

}
}
