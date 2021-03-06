/*
 * Copyright 2015 Follow My Vote, Inc.
 * This file is part of The Follow My Vote Stake-Weighted Voting Application ("SWV").
 *
 * SWV is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * SWV is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with SWV.  If not, see <http://www.gnu.org/licenses/>.
 */

import QtQuick 2.5
import QtQuick.Layouts 1.1
import QtQuick.Controls 1.4
import Qt.labs.settings 1.0
import "CustomControls"

import VPlayApps 1.0

import FollowMyVote.StakeWeightedVoting 1.0

App {
    id: window
    title: Qt.application.name
    visible: true
    minimumWidth: dp(400)

    onInitTheme: {
        Theme.platform = "android"
        Theme.colors.backgroundColor = "#e5e5e5"
    }

    function showError(errorMessage) {
        NativeDialog.confirm(qsTr("Error"), qsTr("An error has occurred:\n%1").arg(errorMessage), function(){}, false)
    }

    Action {
        shortcut: "Ctrl+Q"
        onTriggered: Qt.quit()
    }

    Settings {
        id: appSettings
        property alias windowX: window.x
        property alias windowY: window.y
        property alias windowHeight: window.height
        property alias windowWidth: window.width
    }
    VotingSystem {
       id: _votingSystem

       signal connected

       Component.onCompleted: {
           configureChainAdaptor(false).then(function() {
               loadingOverlay.state = "WALLET_LOADING"
               initialize().then(function() {
                   loadingOverlay.state = "WALLET_UNLOCKING"
               })
           })
       }
       onError: {
           console.log("Error from Voting System: %1".arg(message))
           showError(message.split(";").slice(-1))
       }
       onCurrentAccountChanged: {
           console.log("Current account set to " + currentAccount.name)
           connectToBackend("127.0.0.1", 17073, currentAccount.name).then(function() {
               loadingOverlay.state = "LOADED"
               connected()
           })
       }
    }

    Navigation {
        id: mainNavigation

        NavigationItem {
            title: qsTr("My Feed")
            icon: IconType.newspapero

            NavigationStack {
                 splitView: false
                 ContestListPage {
                    id: feedPage
                    title: qsTr("My Feed")
                    votingSystem: _votingSystem
                    getContestGeneratorFunction: function() {
                        if (votingSystem.isBackendConnected)
                            return votingSystem.backend.getFeedGenerator()
                    }
                    Component.onCompleted: {
                        if (votingSystem.isBackendConnected) loadContests()
                        else votingSystem.connected.connect(loadContests)
                    }
                }
            }
        }
        NavigationItem {
            title: qsTr("My Polls")
            icon: IconType.user

            NavigationStack {
                 splitView: false
                 ContestListPage {
                    id: myContestsPage
                    title: qsTr("My Polls")
                    votingSystem: _votingSystem
                    getContestGeneratorFunction: function() {
                        if (votingSystem.isBackendConnected && votingSystem.currentAccount)
                            return votingSystem.backend.getContestsByCreator(votingSystem.currentAccount.name)
                    }
                    listView.headerPositioning: ListView.PullBackHeader
                    listView.header: CreateContestPlaceholder {
                        votingSystem: _votingSystem
                    }
                }
            }
        }
        NavigationItem {
            title: qsTr("Voted Contests")
            icon: IconType.check

            NavigationStack {
                 splitView: false
                 ContestListPage {
                    id: votedContestsPage
                    title: qsTr("Voted Contests")
                    votingSystem: _votingSystem
                    getContestGeneratorFunction: function() {
                        if (votingSystem.isBackendConnected)
                            return votingSystem.backend.getVotedContests()
                    }
                    listView.headerPositioning: ListView.PullBackHeader

                }
            }
        }
        NavigationItem {
            title: "Coin List"
            icon: IconType.money

            NavigationStack {
                splitView: false
                CoinListPage {
                    id: coinListPage
                    votingSystem: _votingSystem
                }
            }
        }

        NavigationItem {
            title: "Settings"
            icon: IconType.cog

            NavigationStack {
                 splitView: false
                 SettingsPage {
                    votingSystem: _votingSystem
                }
            }
        }
    }

    Rectangle {
        id: loadingOverlay
        anchors.fill: parent
        color: "grey"
        enabled: !!opacity
        state: "WALLET_CONNECTING"

        property alias text: loadingText.text

        MouseArea {
            anchors.fill: parent
            onClicked: { return true }
            acceptedButtons: Qt.AllButtons
            preventStealing: true
        }
        AppText {
            id: loadingText
            anchors.fill: parent
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            wrapMode: Text.WrapAtWordBoundaryOrAnywhere
        }

        states: [
            State {
                name: "LOADING"
                PropertyChanges {
                    target: loadingOverlay
                    opacity: .4
                }
            },
            State {
                name: "WALLET_CONNECTING"
                extend: "LOADING"
                PropertyChanges {
                    target: loadingOverlay
                    text: "Connecting to Wallet..."
                }
            },
            State {
                name: "WALLET_LOADING"
                extend: "LOADING"
                PropertyChanges {
                    target: loadingOverlay
                    text: "Wallet is Initializing..."
                }
            },
            State {
                name: "WALLET_UNLOCKING"
                extend: "LOADING"
                PropertyChanges {
                    target: loadingOverlay
                    text: "Connected to Wallet\nPlease unlock wallet"
                }
            },
            State {
                name: "BACKEND_CONNECTING"
                extend: "LOADING"
                PropertyChanges {
                    target: loadingOverlay
                    text: "Loading..."
                }
            },
            State {
                name: "LOADED"
                PropertyChanges {
                    target: loadingOverlay
                    opacity: 0
                }
            }
        ]
        transitions: [
            Transition {
                from: "*"
                to: "*"
                PropertyAnimation {
                    property: "opacity"
                    duration: 500
                }
                SequentialAnimation {
                    NumberAnimation {
                        target: loadingText
                        property: "opacity"
                        from: 1; to: 0
                    }
                    PropertyAction { target: loadingOverlay; property: "text" }
                    NumberAnimation {
                        target: loadingText
                        property: "opacity"
                        from: 0; to: 1
                    }
                }
            }
        ]
    }
}
