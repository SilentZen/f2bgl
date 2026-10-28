/*
 * Fade To Black engine rewrite
 * Copyright (C) 2006-2012 Gregory Montoir (cyx@users.sourceforge.net)
 */

#ifndef POSTPROCESS_H__
#define POSTPROCESS_H__

class PostProcessor {
public:
	PostProcessor(const char *shader);
	~PostProcessor();
	bool requested() const;
	bool active() const;
	bool init(int width, int height);
	bool resize(int width, int height);
	void beginFrame();
	void endFrame();
	void shutdown();

private:
	struct Impl;
	Impl *_impl;
};

#endif // POSTPROCESS_H__
