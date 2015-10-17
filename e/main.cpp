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
/** @file main.cpp
 * @author Gav Wood <g@ethcore.io>
 * @date 2015
 * Fluidity client.
 */
#include <fstream>
#include <iostream>
#include <ctime>
#include <boost/algorithm/string.hpp>
#include <libp2p/All.h>
#include <libethcore/BasicAuthority.h>
#include <libethereum/Defaults.h>
#include <libethereum/Block.h>
#include <libethereum/Executive.h>
#include <libethereum/Client.h>
#include <libevm/VM.h>
#include <libevm/VMFactory.h>
using namespace std;
using namespace dev;
using namespace eth;

void help()
{
	cout
		<< "Usage e <options>" << endl
		<< endl
		<< "General options:" << endl
		<< "    -V,--version  Show the version and exit." << endl
		<< "    -h,--help  Show this help message and exit." << endl;
	exit(0);
}

void version()
{
	cout << "e version " << dev::Version << endl;
	cout << "By Gav Wood, 2015." << endl;
	cout << "Build: " << DEV_QUOTED(ETH_BUILD_PLATFORM) << "/" << DEV_QUOTED(ETH_BUILD_TYPE) << endl;
	exit(0);
}

enum class Mode
{
	Normal
};


class NetworkFace
{
public:
	/// Get information concerning this node.
	virtual p2p::NodeInfo nodeInfo() const = 0;

	/// Get information on the current peer set.
	virtual std::vector<p2p::PeerSessionInfo> peers() = 0;

	/// Same as peers().size(), but more efficient.
	virtual size_t peerCount() const = 0;

	/// Generalised peer addition.
	virtual void addPeer(p2p::NodeSpec const& _node, p2p::PeerType _t) = 0;

	/// Add node to connect to.
	virtual void addNode(p2p::NodeID const& _node, bi::tcp::endpoint const& _hostEndpoint) = 0;

	/// Require connection to peer.
	virtual void requirePeer(p2p::NodeID const& _node, bi::tcp::endpoint const& _endpoint) = 0;

	/// Save peers
	virtual dev::bytes saveNetwork() = 0;

	/// Sets the ideal number of peers.
	virtual void setIdealPeerCount(size_t _n) = 0;

	virtual bool haveNetwork() const = 0;

	virtual p2p::NetworkPreferences const& networkPreferences() const = 0;
	virtual void setNetworkPreferences(p2p::NetworkPreferences const& _n, bool _dropPeers) = 0;

	virtual p2p::NodeID id() const = 0;

	/// Gets the nodes.
	virtual p2p::Peers nodes() const = 0;

	/// Start the network subsystem.
	virtual void startNetwork() = 0;

	/// Stop the network subsystem.
	virtual void stopNetwork() = 0;

	/// Is network working? there may not be any peers yet.
	virtual bool isNetworkStarted() const = 0;

	/// Get enode string.
	virtual std::string enode() const = 0;
};


class NetworkBase: public NetworkFace
{
public:
	/// Constructor for private instance. If there is already another process on the machine using @a _dbPath, then this will throw an exception.
	/// ethereum() may be safely static_cast()ed to a eth::Client*.
	NetworkBase(
		std::string const& _clientVersion,
		p2p::NetworkPreferences const& _n = p2p::NetworkPreferences(),
		bytesConstRef _network = bytesConstRef()
	):
		m_clientVersion(_clientVersion),
		m_net(_clientVersion, _n, _network)
	{
	}

	/// Destructor.
	~NetworkBase()
	{
		m_net.stop();
	}

	// Misc stuff:

	static std::string composeClientVersion(std::string const& _client, std::string const& _clientName)
	{
		return _client + "-" + "v" + dev::Version + "/" + _clientName + "/" DEV_QUOTED(ETH_BUILD_TYPE) "-" DEV_QUOTED(ETH_BUILD_PLATFORM);
	}

	std::string const& clientVersion() const { return m_clientVersion; }
	void setClientVersion(std::string const& _name) { m_clientVersion = _name; }

	// Network stuff:

	/// Get information on the current peer set.
	std::vector<p2p::PeerSessionInfo> peers() override
	{
		return m_net.peerSessionInfo();
	}

	/// Same as peers().size(), but more efficient.
	size_t peerCount() const override
	{
		return m_net.peerCount();
	}

	/// Generalised peer addition.
	virtual void addPeer(p2p::NodeSpec const& _node, p2p::PeerType _t) override
	{
		m_net.addPeer(_node, _t);
	}

	/// Add node to connect to.
	virtual void addNode(p2p::NodeID const& _node, bi::tcp::endpoint const& _hostEndpoint) override
	{
		m_net.addNode(_node, p2p::NodeIPEndpoint(_hostEndpoint.address(), _hostEndpoint.port(), _hostEndpoint.port()));
	}

	/// Add node to connect to.
	void addNode(p2p::NodeID const& _node, std::string const& _hostString) { addNode(_node, p2p::Network::resolveHost(_hostString)); }

	/// Add node to connect to.
	void addNode(bi::tcp::endpoint const& _endpoint) { addNode(p2p::NodeID(), _endpoint); }

	/// Add node to connect to.
	void addNode(std::string const& _hostString) { addNode(p2p::NodeID(), _hostString); }

	/// Require connection to peer.
	void requirePeer(p2p::NodeID const& _node, bi::tcp::endpoint const& _hostEndpoint) override
	{
		m_net.requirePeer(_node, p2p::NodeIPEndpoint(_hostEndpoint.address(), _hostEndpoint.port(), _hostEndpoint.port()));
	}

	/// Require connection to peer.
	void requirePeer(p2p::NodeID const& _node, std::string const& _hostString) { requirePeer(_node, p2p::Network::resolveHost(_hostString)); }

	/// Save peers
	dev::bytes saveNetwork() override
	{
		return m_net.saveNetwork();
	}

	/// Sets the ideal number of peers.
	void setIdealPeerCount(size_t _n) override
	{
		return m_net.setIdealPeerCount(_n);
	}

	/// Experimental. Sets ceiling for incoming connections to multiple of ideal peer count.
	void setPeerStretch(size_t _n)
	{
		return m_net.setPeerStretch(_n);
	}

	bool haveNetwork() const override { return m_net.haveNetwork(); }

	p2p::NetworkPreferences const& networkPreferences() const override
	{
		return m_net.networkPreferences();
	}

	void setNetworkPreferences(p2p::NetworkPreferences const& _n, bool _dropPeers = false) override
	{
		auto had = isNetworkStarted();
		if (had)
			stopNetwork();
		m_net.setNetworkPreferences(_n, _dropPeers);
		if (had)
			startNetwork();
	}

	p2p::NodeInfo nodeInfo() const override { return m_net.nodeInfo(); }

	p2p::NodeID id() const override { return m_net.id(); }

	std::string enode() const override { return m_net.enode(); }

	/// Gets the nodes.
	p2p::Peers nodes() const override { return m_net.getPeers(); }

	/// Start the network subsystem.
	void startNetwork() override { m_net.start(); }

	/// Stop the network subsystem.
	void stopNetwork() override { m_net.stop(); }

	/// Is network working? there may not be any peers yet.
	bool isNetworkStarted() const override { return m_net.isStarted(); }

protected:
	std::string m_clientVersion;					///< Our end-application client's name/version.

	p2p::Host m_net;								///< Should run in background and send us events when blocks found and allow us to send blocks as required.
};

class Ethereum: public NetworkBase
{
public:
	Ethereum(
		std::string const& _clientVersion,
		std::string const& _dbPath,
		WithExisting _we = WithExisting::Trust,
		p2p::NetworkPreferences const& _n = p2p::NetworkPreferences(),
		bytesConstRef _network = bytesConstRef()
	):
		NetworkBase(_clientVersion, _n, _network)
	{
		if (_dbPath.size())
			Defaults::setDBPath(_dbPath);
		m_ethereum.reset(new eth::SpecialisedClient<BasicAuthority>(&m_net, shared_ptr<GasPricer>(), _dbPath, _we));
	}

	~Ethereum()
	{
		m_ethereum.reset();
	}

	// The mainline interfaces:

	eth::Client* ethereum() const { return m_ethereum.get(); }

private:
	std::shared_ptr<eth::Client> m_ethereum;		///< Client for Ethereum ("eth") protocol.
};

int main(int argc, char** argv)
{
//	Mode mode = Mode::Normal;

	for (int i = 1; i < argc; ++i)
	{
		string arg = argv[i];
		if (arg == "-h" || arg == "--help")
			help();
		else if (arg == "-V" || arg == "--version")
			version();
	}

	Ethereum eth("e", getDataDir("ethereum-e"));

	while (true)
	{
		this_thread::sleep_for(chrono::seconds(1));
	}

	return 0;
}
