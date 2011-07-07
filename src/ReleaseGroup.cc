/* --------------------------------------------------------------------------

   libmusicbrainz4 - Client library to access MusicBrainz

   Copyright (C) 2011 Andrew Hawkins

   This file is part of libmusicbrainz4.

   This library is free software; you can redistribute it and/or
   modify it under the terms of v2 of the GNU Lesser General Public
   License as published by the Free Software Foundation.

   Flactag is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

     $Id$

----------------------------------------------------------------------------*/

#include "musicbrainz4/ReleaseGroup.h"

#include "musicbrainz4/ArtistCredit.h"
#include "musicbrainz4/Rating.h"
#include "musicbrainz4/UserRating.h"
#include "musicbrainz4/Release.h"
#include "musicbrainz4/Relation.h"
#include "musicbrainz4/Tag.h"
#include "musicbrainz4/UserTag.h"

class MusicBrainz4::CReleaseGroupPrivate
{
	public:
		CReleaseGroupPrivate()
		:	m_ArtistCredit(0),
			m_ReleaseList(0),
			m_RelationList(0),
			m_TagList(0),
			m_UserTagList(0),
			m_Rating(0),
			m_UserRating(0)
		{
		}

		std::string m_ID;
		std::string m_Type;
		std::string m_FirstReleaseDate;
		std::string m_Title;
		std::string m_Comment;
		CArtistCredit *m_ArtistCredit;
		CGenericList<CRelease> *m_ReleaseList;
		CGenericList<CRelation> *m_RelationList;
		CGenericList<CTag> *m_TagList;
		CGenericList<CUserTag> *m_UserTagList;
		CRating *m_Rating;
		CUserRating *m_UserRating;
};

MusicBrainz4::CReleaseGroup::CReleaseGroup(const XMLNode& Node)
:	m_d(new CReleaseGroupPrivate)
{
	if (!Node.isEmpty())
	{
		//std::cout << "Name credit node: " << std::endl << Node.createXMLString(true) << std::endl;

		if (Node.isAttributeSet("id"))
			m_d->m_ID=Node.getAttribute("id");

		if (Node.isAttributeSet("type"))
			m_d->m_Type=Node.getAttribute("type");

		if (Node.isAttributeSet("first-release-date"))
			m_d->m_FirstReleaseDate=Node.getAttribute("first-release-date");

		for (int count=0;count<Node.nChildNode();count++)
		{
			XMLNode ChildNode=Node.getChildNode(count);
			std::string NodeName=ChildNode.getName();
			std::string NodeValue;
			if (ChildNode.getText())
				NodeValue=ChildNode.getText();

			if ("title"==NodeName)
			{
				m_d->m_Title=NodeValue;
			}
			else if ("comment"==NodeName)
			{
				m_d->m_Comment=NodeValue;
			}
			else if ("artist-credit"==NodeName)
			{
				m_d->m_ArtistCredit=new CArtistCredit(ChildNode);
			}
			else if ("release-list"==NodeName)
			{
				m_d->m_ReleaseList=new CGenericList<CRelease>(ChildNode,"release");
			}
			else if ("relation-list"==NodeName)
			{
				m_d->m_RelationList=new CGenericList<CRelation>(ChildNode,"relation");
			}
			else if ("tag-list"==NodeName)
			{
				m_d->m_TagList=new CGenericList<CTag>(ChildNode,"tag");
			}
			else if ("user-tag-list"==NodeName)
			{
				m_d->m_UserTagList=new CGenericList<CUserTag>(ChildNode,"user-tag");
			}
			else if ("rating"==NodeName)
			{
				m_d->m_Rating=new CRating(ChildNode);
			}
			else if ("user-rating"==NodeName)
			{
				m_d->m_UserRating=new CUserRating(ChildNode);
			}
			else
			{
				std::cerr << "Unrecognised release group node: '" << NodeName << "'" << std::endl;
			}
		}
	}
}

MusicBrainz4::CReleaseGroup::CReleaseGroup(const CReleaseGroup& Other)
:	m_d(new CReleaseGroupPrivate)
{
	*this=Other;
}

MusicBrainz4::CReleaseGroup& MusicBrainz4::CReleaseGroup::operator =(const CReleaseGroup& Other)
{
	if (this!=&Other)
	{
		Cleanup();

		m_d->m_ID=Other.m_d->m_ID;
		m_d->m_Type=Other.m_d->m_Type;
		m_d->m_FirstReleaseDate=Other.m_d->m_FirstReleaseDate;
		m_d->m_Title=Other.m_d->m_Title;
		m_d->m_Comment=Other.m_d->m_Comment;

		if (Other.m_d->m_ArtistCredit)
			m_d->m_ArtistCredit=new CArtistCredit(*Other.m_d->m_ArtistCredit);

		if (Other.m_d->m_ReleaseList)
			m_d->m_ReleaseList=new CGenericList<CRelease>(*Other.m_d->m_ReleaseList);

		if (Other.m_d->m_RelationList)
			m_d->m_RelationList=new CGenericList<CRelation>(*Other.m_d->m_RelationList);

		if (Other.m_d->m_TagList)
			m_d->m_TagList=new CGenericList<CTag>(*Other.m_d->m_TagList);

		if (Other.m_d->m_UserTagList)
			m_d->m_UserTagList=new CGenericList<CUserTag>(*Other.m_d->m_UserTagList);

		if (Other.m_d->m_Rating)
			m_d->m_Rating=new CRating(*Other.m_d->m_Rating);

		if (Other.m_d->m_UserRating)
			m_d->m_UserRating=new CUserRating(*Other.m_d->m_UserRating);
	}

	return *this;
}

MusicBrainz4::CReleaseGroup::~CReleaseGroup()
{
	Cleanup();

	delete m_d;
}

void MusicBrainz4::CReleaseGroup::Cleanup()
{
	delete m_d->m_ArtistCredit;
	m_d->m_ArtistCredit=0;

	delete m_d->m_ReleaseList;
	m_d->m_ReleaseList=0;

	delete m_d->m_RelationList;
	m_d->m_RelationList=0;

	delete m_d->m_TagList;
	m_d->m_TagList=0;

	delete m_d->m_UserTagList;
	m_d->m_UserTagList=0;

	delete m_d->m_Rating;
	m_d->m_Rating=0;

	delete m_d->m_UserRating;
	m_d->m_UserRating=0;
}

std::string MusicBrainz4::CReleaseGroup::ID() const
{
	return m_d->m_ID;
}

std::string MusicBrainz4::CReleaseGroup::Type() const
{
	return m_d->m_Type;
}

std::string MusicBrainz4::CReleaseGroup::FirstReleaseDate() const
{
	return m_d->m_FirstReleaseDate;
}

std::string MusicBrainz4::CReleaseGroup::Title() const
{
	return m_d->m_Title;
}

std::string MusicBrainz4::CReleaseGroup::Comment() const
{
	return m_d->m_Comment;
}

MusicBrainz4::CArtistCredit *MusicBrainz4::CReleaseGroup::ArtistCredit() const
{
	return m_d->m_ArtistCredit;
}

MusicBrainz4::CGenericList<MusicBrainz4::CRelease> *MusicBrainz4::CReleaseGroup::ReleaseList() const
{
	return m_d->m_ReleaseList;
}

MusicBrainz4::CGenericList<MusicBrainz4::CRelation> *MusicBrainz4::CReleaseGroup::RelationList() const
{
	return m_d->m_RelationList;
}

MusicBrainz4::CGenericList<MusicBrainz4::CTag> *MusicBrainz4::CReleaseGroup::TagList() const
{
	return m_d->m_TagList;
}

MusicBrainz4::CGenericList<MusicBrainz4::CUserTag> *MusicBrainz4::CReleaseGroup::UserTagList() const
{
	return m_d->m_UserTagList;
}

MusicBrainz4::CRating *MusicBrainz4::CReleaseGroup::Rating() const
{
	return m_d->m_Rating;
}

MusicBrainz4::CUserRating *MusicBrainz4::CReleaseGroup::UserRating() const
{
	return m_d->m_UserRating;
}

std::ostream& operator << (std::ostream& os, const MusicBrainz4::CReleaseGroup& ReleaseGroup)
{
	os << "Release group:" << std::endl;

	os << "\tID:                 " << ReleaseGroup.ID() << std::endl;
	os << "\tType:               " << ReleaseGroup.Type() << std::endl;
	os << "\tFirst release date: " << ReleaseGroup.FirstReleaseDate() << std::endl;
	os << "\tTitle:              " << ReleaseGroup.Title() << std::endl;
	os << "\tComment:            " << ReleaseGroup.Comment() << std::endl;

	if (ReleaseGroup.ArtistCredit())
		os << *ReleaseGroup.ArtistCredit() << std::endl;

	if (ReleaseGroup.ReleaseList())
		os << *ReleaseGroup.ReleaseList() << std::endl;

	if (ReleaseGroup.RelationList())
		os << *ReleaseGroup.RelationList() << std::endl;

	if (ReleaseGroup.TagList())
		os << *ReleaseGroup.TagList() << std::endl;

	if (ReleaseGroup.UserTagList())
		os << *ReleaseGroup.UserTagList() << std::endl;

	if (ReleaseGroup.Rating())
		os << *ReleaseGroup.Rating() << std::endl;

	if (ReleaseGroup.UserRating())
		os << *ReleaseGroup.UserRating() << std::endl;

	return os;
}

