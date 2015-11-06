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
/** @file canonblockchain.cpp
 * @author Dimitry Khokhlov <dimitry@ethdev.com>
 * @date 2015
 * CanonBlockChain test functions.
 */

#include <libethereum/CanonBlockChain.h>
#include <libdevcore/TransientDirectory.h>
#include <libethereum/Block.h>
#include <test/TestHelper.h>

using namespace std;
using namespace dev;
using namespace dev::eth;

std::string const c_testGenesis = R"(
{
	"nonce": "0x000000000000012a",
	"difficulty": "0x20011",
	"mixhash": "0x0000000000000000000000000000000000000000000000000000000000000001",
	"coinbase": "0x8888f1f195afa192cfee860698584c030f4c9db1",
	"timestamp": "0x10",
	"parentHash": "0x0000000000000000000000000000000000000000000000000000000000000000",
	"extraData": "0x234323",
	"gasLimit": "0x6000",
	"alloc": {
		"0000000000000000000000000000000000000001": { "wei": "2" },
		"0000000000000000000000000000000000000002": { "wei": "2" },
		"0000000000000000000000000000000000000003": { "wei": "2" },
		"0000000000000000000000000000000000000004": { "wei": "2" },
		"abdbdb2cbd23b783741e8d7fcf51e459b497e4a6": { "wei": "1606938044258990275541962092341162602522202993782792835301376" },
		"e6716f9544a56c530d868e4bfbacb172315bdead": { "wei": "1606938044258990275541962092341162602522202993782792835301376" },
		"b9c015918bdaba24b4ff057a92a3873d6eb201be": { "wei": "1606938044258990275541962092341162602522202993782792835301376" },
		"1a26338f0d905e295fccb71fa9ea849ffa12aaf4": { "wei": "1606938044258990275541962092341162602522202993782792835301376" },
		"2ef47100e0787b915105fd5e3f4ff6752079d5cb": { "wei": "1606938044258990275541962092341162602522202993782792835301376" },
		"cd2a3d9f938e13cd947ec05abc7fe734df8dd826": { "wei": "1606938044258990275541962092341162602522202993782792835301376" },
		"6c386a4b26f73c802f34673f7248bb118f97424a": { "wei": "1606938044258990275541962092341162602522202993782792835301376" },
		"e4157b34ea9615cfbde6b4fda419828124b70c78": { "wei": "1606938044258990275541962092341162602522202993782792835301376" }
	}
}
)";

BOOST_AUTO_TEST_SUITE(CanonBlockChainSuite)

BOOST_AUTO_TEST_CASE(genesis)
{
	TransientDirectory tDir;
	CanonBlockChain<Ethash> cBc(tDir.path());

	cBc.forceGenesisDifficulty(3151616);
	cBc.forceGenesisExtraData(fromHex("32abcd"));
	cBc.forceGenesisGasLimit(20000);

	bytes genBytes = cBc.createGenesisBlock();
	BlockInfo a(genBytes);

	BOOST_REQUIRE(a.difficulty() == 3151616);
	BOOST_REQUIRE(a.gasLimit() == 20000);
	BOOST_REQUIRE(a.extraData() == fromHex("32abcd"));

	cBc.setGenesis(c_testGenesis);  //force functions outbid this directive!!!
	genBytes = cBc.createGenesisBlock();	
	Ethash::BlockHeader bh(genBytes);
	BlockInfo const& bi = (BlockInfo)(bh);

	BOOST_REQUIRE(bi.parentHash() == h256("0000000000000000000000000000000000000000000000000000000000000000"));
	BOOST_REQUIRE(bi.sha3Uncles() == h256("1dcc4de8dec75d7aab85b567b6ccd41ad312451b948a7413f0a142fd40d49347"));
	BOOST_REQUIRE(bi.beneficiary() == Address("0x8888f1f195afa192cfee860698584c030f4c9db1"));
	BOOST_REQUIRE(bi.transactionsRoot() == h256("0x56e81f171bcc55a6ff8345e692c0f86e5b48e01b996cadc001622fb5e363b421"));
	BOOST_REQUIRE(bi.receiptsRoot() == h256("0x56e81f171bcc55a6ff8345e692c0f86e5b48e01b996cadc001622fb5e363b421"));
	BOOST_REQUIRE(bi.logBloom() == h2048("00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"));
	//BOOST_REQUIRE(b.difficulty() == 131089);
	BOOST_REQUIRE(bi.number() == 0);
	//BOOST_REQUIRE(bh.gasLimit() == 24576);
	//BOOST_REQUIRE(b.extraData() == fromHex("0x234323"));
	BOOST_REQUIRE(bh.timestamp() == 16);
	BOOST_REQUIRE(bh.mixHash() == h256("0x0000000000000000000000000000000000000000000000000000000000000001"));
	BOOST_REQUIRE(bh.nonce() == h64("0x000000000000012a"));
}

BOOST_AUTO_TEST_CASE(simpleMining)
{
	TransientDirectory tDir;
	CanonBlockChain<Ethash>::forceGenesisDifficulty(131072);
	CanonBlockChain<Ethash> cBc(tDir.path());

	OverlayDB const& genesisDB = OverlayDB(State::openDB(tDir.path(), h256{}, WithExisting::Kill));
	Block block = cBc.genesisBlock(genesisDB);
	try
	{
		block.sync(cBc);
		dev::eth::mine(block, cBc);
	}
	catch(...)
	{
		BOOST_ERROR("Error when mining a simple block on canonBlockchain!");
	}

	cBc.import(block.blockData(), block.db());
	BOOST_REQUIRE(BlockInfo(cBc.block()) == block.info());
}

BOOST_AUTO_TEST_SUITE_END()
